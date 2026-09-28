// TCLSLANG BY BEN STAHL
// https://github.com/benastahl/tclslang

#include "tclslang/design.hpp"

#include <algorithm>

#include <slang/ast/Expression.h>
#include <slang/ast/expressions/AssignmentExpressions.h>
#include <slang/ast/symbols/BlockSymbols.h>
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/symbols/MemberSymbols.h>
#include <slang/ast/symbols/PortSymbols.h>
#include <slang/ast/symbols/VariableSymbols.h>
#include <slang/ast/types/AllTypes.h>
#include <slang/ast/types/NetType.h>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/diagnostics/TextDiagnosticClient.h>
#include <slang/syntax/AllSyntax.h>

using namespace slang::ast;
using slang::syntax::SyntaxTree;

namespace tclslang {

namespace {

// Trims leading/trailing whitespace (syntax text includes source trivia).
std::string trimmed(std::string_view text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return "";
    const auto end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(begin, end - begin + 1));
}

// Source text of an expression, e.g. "bus[3:2]" or "{a, b}". Uses the source
// range rather than the syntax node, because slang synthesizes nodes without
// syntax (implicit conversions, per-element array connections).
std::string exprText(const Expression& expr, const slang::SourceManager* sm) {
    const auto range = expr.sourceRange;
    if (sm && range.start().valid() && range.start().buffer() == range.end().buffer()) {
        const auto text = sm->getSourceText(range.start().buffer());
        const auto begin = range.start().offset(), end = range.end().offset();
        if (begin <= end && end <= text.size()) return trimmed(text.substr(begin, end - begin));
    }
    return expr.syntax ? trimmed(expr.syntax->toString()) : "";
}

std::string hierPath(const Symbol& symbol) {
    std::string path;
    symbol.getHierarchicalPath(path);
    return path;
}

// Collects the instances directly below `scope`, looking through generate
// blocks and instance arrays but not into other instances' bodies.
void collectInstances(const Scope& scope, std::vector<const InstanceSymbol*>& out);

// Path of `child` relative to `parent`'s body, e.g. "g_loop[0].u_gen". Both
// paths go through the body, which slang may share between identical
// instances, so the prefix comes from the body too.
std::string relativePath(const InstanceSymbol& parent, const InstanceSymbol& child) {
    const std::string prefix = hierPath(parent.body) + ".";
    std::string path = hierPath(child);
    if (path.starts_with(prefix)) path.erase(0, prefix.size());
    return path;
}

std::string_view directionName(ArgumentDirection direction) {
    switch (direction) {
        case ArgumentDirection::In: return "input";
        case ArgumentDirection::Out: return "output";
        case ArgumentDirection::InOut: return "inout";
        case ArgumentDirection::Ref: return "ref";
    }
    return "";
}

void collectInstances(const Scope& scope, std::vector<const InstanceSymbol*>& out) {
    for (const Symbol& member : scope.members()) {
        switch (member.kind) {
            case SymbolKind::Instance:
                out.push_back(&member.as<InstanceSymbol>());
                break;
            case SymbolKind::InstanceArray:
                collectInstances(member.as<InstanceArraySymbol>(), out);
                break;
            case SymbolKind::GenerateBlock:
                // Blocks whose condition is false are still in the AST.
                if (!member.as<GenerateBlockSymbol>().isUninstantiated)
                    collectInstances(member.as<GenerateBlockSymbol>(), out);
                break;
            case SymbolKind::GenerateBlockArray:
                collectInstances(member.as<GenerateBlockArraySymbol>(), out);
                break;
            default:
                break;
        }
    }
}

} // namespace

// --- Port -------------------------------------------------------------------

Port::Port(Tree& tree, Object* parent, const Symbol& symbol) :
    Object(Kind::Port, tree, parent, &symbol), symbol(symbol), name(symbol.name) {

    if (symbol.kind == SymbolKind::InterfacePort) {
        const auto& port = symbol.as<InterfacePortSymbol>();
        kind = "interface";
        interfaceName = port.interfaceDef ? std::string(port.interfaceDef->name) : "interface";
        modport = std::string(port.modport);
        return;
    }
    if (symbol.kind == SymbolKind::MultiPort) {
        const auto& port = symbol.as<MultiPortSymbol>();
        kind = "multiport";
        direction = directionName(port.direction);
        dataType = port.getType().toString();
        width = port.getType().getBitstreamWidth();
        return;
    }

    const auto& port = symbol.as<PortSymbol>();
    direction = directionName(port.direction);
    dataType = port.getType().toString();
    width = port.getType().getBitstreamWidth();

    // The internal symbol is what the port connects to inside the module: a
    // net (`input wire a`, `input a`) or a variable (`input logic a`,
    // `output reg a`). Implicit port expressions have none.
    if (port.internalSymbol && port.internalSymbol->kind == SymbolKind::Variable) {
        kind = "var";
    } else {
        kind = "net";
        if (port.internalSymbol && port.internalSymbol->kind == SymbolKind::Net)
            netType = port.internalSymbol->as<NetSymbol>().netType.name;
    }

    // Outermost dimension first, e.g. `logic [3:0] a [0:15]` -> {0 15} {3 0}.
    const Type* type = &port.getType().getCanonicalType();
    while (type->isArray()) {
        if (type->isPackedArray()) {
            const auto& packed = type->as<PackedArrayType>();
            dimensions.push_back({packed.range.left, packed.range.right});
            type = &packed.elementType.getCanonicalType();
        } else if (type->kind == SymbolKind::FixedSizeUnpackedArrayType) {
            const auto& unpacked = type->as<FixedSizeUnpackedArrayType>();
            dimensions.push_back({unpacked.range.left, unpacked.range.right});
            type = &unpacked.elementType.getCanonicalType();
        } else {
            break;
        }
    }
}

// --- Driver -----------------------------------------------------------------

Driver::Driver(Tree& tree, Object* parent, const void* key, std::string kind) :
    Object(Kind::Driver, tree, parent, key), kind(std::move(kind)) {}

std::string Driver::name() const {
    return symbol ? std::string(symbol->name) : "";
}

std::string Driver::text() const {
    if (!expr) return "";
    return exprText(*expr, &tree.sources());
}

std::string Driver::dataType() const {
    if (kind == "var" || kind == "net") return symbol->as<ValueSymbol>().getType().toString();
    if (expr) return expr->type->toString();
    return "";
}

std::string Driver::netType() const {
    return kind == "net" ? std::string(symbol->as<NetSymbol>().netType.name) : "";
}

// --- Connection -------------------------------------------------------------

Connection::Connection(Tree& tree, Object* parent, const PortConnection& conn) :
    Object(Kind::Connection, tree, parent, &conn), conn(conn) {}

std::string Connection::name() const {
    return std::string(conn.port.name);
}

Port* Connection::port() {
    // Keyed under the owning instance, so this is the same object that
    // `$cell get_ports` returns for this port.
    return tree.intern<Port>(Kind::Port, parent, &conn.port, [&] {
        return std::make_unique<Port>(tree, parent, conn.port);
    });
}

Driver* Connection::driver() {
    const auto make = [&](std::string kind) {
        return tree.intern<Driver>(Kind::Driver, this, &conn, [&] {
            return std::make_unique<Driver>(tree, this, &conn, std::move(kind));
        });
    };

    if (conn.port.kind == SymbolKind::InterfacePort) {
        auto [ifaceSymbol, modport] = conn.getIfaceConn();
        if (!ifaceSymbol) return nullptr;

        Driver* driver = make("interface");
        driver->symbol = ifaceSymbol;
        if (modport) driver->modport = std::string(modport->name);
        return driver;
    }

    const Expression* expr = conn.getExpression();
    if (!expr || expr->bad()) return nullptr;

    // Output and inout connections are bound as an assignment whose
    // left-hand side is the connected expression.
    if (expr->kind == ExpressionKind::Assignment) expr = &expr->as<AssignmentExpression>().left();

    const Symbol* symbol = expr->getSymbolReference();
    Driver* driver;
    if (expr->constant) {
        driver = make("const");
        driver->constant = expr->constant->toString();
    } else if (symbol && symbol->kind == SymbolKind::Net) {
        driver = make("net");
        driver->symbol = symbol;
    } else if (symbol && symbol->kind == SymbolKind::Variable) {
        driver = make("var");
        driver->symbol = symbol;
    } else {
        driver = make("expr");
    }
    driver->expr = expr;
    return driver;
}

// --- Instance ---------------------------------------------------------------

Instance::Instance(Kind kind, Tree& tree, Object* parent, const InstanceSymbol& symbol,
                   std::string name, std::string path) :
    Object(kind, tree, parent, &symbol), symbol(symbol), name(std::move(name)),
    path(std::move(path)) {}

std::string Instance::refName() const {
    return std::string(symbol.getDefinition().name);
}

std::vector<Port*> Instance::ports() {
    std::vector<Port*> result;
    for (const Symbol* portSymbol : symbol.body.getPortList()) {
        result.push_back(tree.intern<Port>(Kind::Port, this, portSymbol, [&] {
            return std::make_unique<Port>(tree, this, *portSymbol);
        }));
    }
    return result;
}

std::vector<Instance*> Instance::cells() {
    std::vector<const InstanceSymbol*> children;
    collectInstances(symbol.body, children);

    // Cells are named by their path below this instance, so instances in
    // generate blocks and arrays read as "g_loop[0].u_gen" or "u[1]".
    std::vector<Instance*> result;
    for (const InstanceSymbol* child : children) {
        result.push_back(tree.intern<Instance>(Kind::Cell, this, child, [&] {
            std::string name = relativePath(symbol, *child);
            std::string fullPath = path + "." + name;
            return std::make_unique<Instance>(Kind::Cell, tree, this, *child, std::move(name),
                                              std::move(fullPath));
        }));
    }
    return result;
}

std::vector<Connection*> Instance::connections() {
    std::vector<Connection*> result;
    for (const PortConnection* conn : symbol.getPortConnections()) {
        result.push_back(tree.intern<Connection>(Kind::Connection, this, conn, [&] {
            return std::make_unique<Connection>(tree, this, *conn);
        }));
    }
    return result;
}

// --- Tree -------------------------------------------------------------------

Tree::Tree() : Object(Kind::Tree, *this, nullptr, nullptr) {}

std::unique_ptr<Tree> Tree::parse(const std::vector<std::string>& paths, std::string& error) {
    std::unique_ptr<Tree> tree(new Tree());

    std::vector<std::string_view> views(paths.begin(), paths.end());
    auto result = SyntaxTree::fromFiles(views, tree->sourceManager);
    if (!result) {
        const auto& [code, path] = result.error();
        error = "cannot read \"" + std::string(path) + "\": " + code.message();
        return nullptr;
    }
    tree->syntax = *result;
    tree->compilation = std::make_unique<Compilation>();
    tree->compilation->addSyntaxTree(tree->syntax);

    // Forces full elaboration and collects everything slang reports.
    slang::DiagnosticEngine engine(tree->sourceManager);
    auto client = std::make_shared<slang::TextDiagnosticClient>();
    engine.addClient(client);
    for (const auto& diag : tree->compilation->getAllDiagnostics()) engine.issue(diag);
    tree->diagText = client->getString();

    if (engine.getNumErrors() > 0) {
        error = "design has " + std::to_string(engine.getNumErrors()) + " error(s)\n" +
                tree->diagText;
        return nullptr;
    }
    return tree;
}

Instance* Tree::findModule(std::string_view moduleName) {
    // Breadth-first from the tops, so the shallowest instance wins.
    std::vector<std::pair<const InstanceSymbol*, std::string>> queue;
    for (const InstanceSymbol* top : compilation->getRoot().topInstances)
        queue.emplace_back(top, std::string(top->name));

    for (size_t i = 0; i < queue.size(); i++) {
        const auto [inst, instPath] = queue[i];
        if (inst->getDefinition().name == moduleName) {
            return intern<Instance>(Kind::Module, this, inst, [&] {
                return std::make_unique<Instance>(Kind::Module, *this, this, *inst,
                                                  std::string(moduleName), instPath);
            });
        }

        std::vector<const InstanceSymbol*> children;
        collectInstances(inst->body, children);
        for (const InstanceSymbol* child : children)
            queue.emplace_back(child, instPath + "." + relativePath(*inst, *child));
    }
    return nullptr;
}

std::vector<std::string> Tree::topModules() const {
    std::vector<std::string> names;
    for (const InstanceSymbol* top : compilation->getRoot().topInstances)
        names.emplace_back(top->getDefinition().name);
    return names;
}

void Tree::release(Object* object) {
    auto& siblings = object->parent->children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), object), siblings.end());
    objects.erase({object->kind, object->parent, object->key});
}

} // namespace tclslang
