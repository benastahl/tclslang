//
// Created by ec2-user on 4/10/25.
//

// HARDWARE DESCRIPTION LANGUAGE TREE FOR TCLSLANG BY BEN STAHL
// https://github.com/benastahl/tclslang

#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxNode.h"
#include <slang/ast/symbols/PortSymbols.h>
#include <slang/ast/types/Type.h>
#include <slang/ast/types/AllTypes.h>
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/Compilation.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/symbols/VariableSymbols.h>
#include <iostream>
#include <cstring>
#include <string>
#include <utility>
#include <tcl.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/ast/expressions/AssignmentExpressions.h>
#include <slang/ast/symbols/BlockSymbols.h>
#include <slang/ast/symbols/MemberSymbols.h>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/diagnostics/TextDiagnosticClient.h>
#include <slang/text/SourceManager.h>

using namespace slang::syntax;
using namespace slang::ast;
using namespace std;

#ifndef TCLSLANG_HDL_TREE_H
#define TCLSLANG_HDL_TREE_H

class Instance;
class Port;
class Module;
class Tree;
class Cell;
class PortConn;
class Driver;

unordered_map<string, unique_ptr<Port>>     ports;
unordered_map<string, unique_ptr<Module>>   modules;
unordered_map<string, unique_ptr<Tree>>     trees;
unordered_map<string, unique_ptr<Cell>>     cells;
unordered_map<string, unique_ptr<PortConn>> connections;
unordered_map<string, unique_ptr<Driver>>   drivers;

static int cellCounter  = 0;
static int portCounter  = 0;
static int connCounter  = 0;
static int driverCounter = 0;


// Trims leading/trailing whitespace (syntax text includes source trivia).
inline string trimmed(const string& text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == string::npos) return "";
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

// Source text of an expression, e.g. "bus[3:2]" or "{a, b}". Uses the source
// range rather than the syntax node, because slang synthesizes nodes without
// syntax (implicit conversions, per-element array connections).
inline string exprText(const Expression* expr, const slang::SourceManager* sm) {
    if (!expr) return "";
    const auto range = expr->sourceRange;
    if (sm && range.start().valid() && range.start().buffer() == range.end().buffer()) {
        const auto text = sm->getSourceText(range.start().buffer());
        const auto begin = range.start().offset(), end = range.end().offset();
        if (begin <= end && end <= text.size()) {
            return trimmed(string(text.substr(begin, end - begin)));
        }
    }
    return expr->syntax ? trimmed(expr->syntax->toString()) : "";
}

// Collects the instances directly below `scope`, looking through generate
// blocks and instance arrays but not into other instances' bodies.
inline void collectInstances(const Scope& scope, vector<const InstanceSymbol*>& out) {
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

inline string hierPath(const Symbol& symbol) {
    string path;
    symbol.getHierarchicalPath(path);
    return path;
}

// Just adds port to list of port instances for use in tcl.
void addPort(const Symbol* portSymbol, string* handleStr) {
    auto port = make_unique<Port>(portSymbol);

    // Generate a unique handle
    *handleStr = "port" + std::to_string(portCounter++);

    // Store the instance in the map
    ports[*handleStr] = std::move(port);
}


// What a port connection is hooked up to. `type` is one of:
//   const      a constant expression (literal, parameter)
//   var / net  a reference to (a select of) a single variable or net
//   expr       any other expression, e.g. {a, b} or a & b
//   interface  an interface instance or modport on an interface port
class Driver {
public:
    string type;
    const Symbol* driverSymbol;
    string constant;
    const Expression* expr = nullptr;
    const slang::SourceManager* sourceManager = nullptr;
    string modport;

    explicit Driver(const Symbol* driverSymbol, string type, string constant = "") {
        this->type = std::move(type);
        this->driverSymbol = driverSymbol;
        this->constant = std::move(constant);
    }

};

class PortConn {
public:
    const PortConnection* portConn = nullptr;
    const Port* port = nullptr;
    const Driver* driver = nullptr;
    string portHandle;
    string driverHandle;

    explicit PortConn(const PortConnection* portConn) {
        this->portConn = portConn;
        addPort(&portConn->port, &portHandle); // adds port to port handles list for use in tcl
        this->port = ports[portHandle].get(); // sets this->port to a Port class for use

        setDriver(&driverHandle); // sets this->driver to value

    }

    void storeDriver(unique_ptr<Driver> driver, string* handleStr) {
        *handleStr = "driver" + std::to_string(driverCounter++);
        this->driver = driver.get();
        drivers[*handleStr] = std::move(driver);
    }

    void setDriver(string* handleStr) {
        if (portConn->port.kind == SymbolKind::InterfacePort) {
            auto [ifaceSymbol, modport] = portConn->getIfaceConn();
            if (!ifaceSymbol) return;

            auto driver = make_unique<Driver>(ifaceSymbol, "interface");
            if (modport) driver->modport = string(modport->name);
            storeDriver(std::move(driver), handleStr);
            return;
        }

        const Expression* expr = this->portConn->getExpression();

        // checking for valid expression.
        if (!expr) {
            return;
        } else if (expr->bad()) {
            return;
        }

        // Output and inout connections are bound as an assignment whose
        // left-hand side is the connected expression.
        if (expr->kind == ExpressionKind::Assignment) {
            expr = &expr->as<AssignmentExpression>().left();
        }

        unique_ptr<Driver> driver;
        const Symbol* symb = expr->getSymbolReference();

        if (expr->constant) {
            driver = make_unique<Driver>(nullptr, "const", expr->constant->toString());
        } else if (symb && symb->kind == SymbolKind::Net) {
            driver = make_unique<Driver>(symb, "net");
        } else if (symb && symb->kind == SymbolKind::Variable) {
            driver = make_unique<Driver>(symb, "var");
        } else {
            driver = make_unique<Driver>(nullptr, "expr");
        }

        driver->expr = expr;
        driver->sourceManager = portConn->port.getParentScope()->getCompilation().getSourceManager();
        storeDriver(std::move(driver), handleStr);
    }

};

class Port {
public:
    const Symbol *port;
    string portType; // Net, Variable, Interface or MultiPort
    string direction;
    string decType;
    string dimType;
    vector<array<int, 2>> dimensions;

    explicit Port(const Symbol *symbol) {
        this->port = symbol;

        if (symbol->kind == SymbolKind::InterfacePort) {
            this->portType = "Interface";
            return;
        }
        if (symbol->kind == SymbolKind::MultiPort) {
            this->portType = "MultiPort";
            this->direction = toString(symbol->as<MultiPortSymbol>().direction);
            return;
        }

        const auto* portSymbol = &symbol->as<PortSymbol>();
        this->direction = toString(portSymbol->direction);
        this->portType = portSymbol->internalSymbol ? string(toString(portSymbol->internalSymbol->kind))
                                                    : "Net";  // implicit (unnamed) port expression

        // Determine if it's wire or reg
        if (portSymbol->isNetPort() || portSymbol->direction == ArgumentDirection::In ||
            portSymbol->direction == ArgumentDirection::InOut) {
            this->decType = "wire";
        } else {
            this->decType = "reg";
        }

        // Get the port's type and check if it's an array
        const Type *type = &portSymbol->getType();

        while (type->isArray()) {
            if (type->isPackedArray()) {
                const auto &packedArray = type->getCanonicalType().as<PackedArrayType>();
                this->dimensions.push_back({packedArray.range.left, packedArray.range.right});

                type = &packedArray.elementType;
            } else if (type->isUnpackedArray()) {
                const auto &unpackedArray = type->getCanonicalType().as<FixedSizeUnpackedArrayType>();
                this->dimensions.push_back({unpackedArray.range.left, unpackedArray.range.right});

                type = &unpackedArray.elementType;
            } else {
                break;
            }
        }
    }

};

class Instance {
public:
    string name;
    const InstanceSymbol* instSymbol = nullptr;

    explicit Instance(const InstanceSymbol* instSymbol, string name = "") {
        this->instSymbol = instSymbol;
        this->name = name.empty() ? string(this->instSymbol->name) : std::move(name);
    }

    vector<string> getPorts() const {
        vector<string> portHandles;

        for (const Symbol* portSymbol: this->instSymbol->body.getPortList()) {
            string portHandle;
            addPort(portSymbol, &portHandle);

            if (portHandle.empty()) continue;

            portHandles.push_back(portHandle);
        }

        return portHandles;
    }

    vector<string> getCells() const {
        vector<string> cellHandles;

        vector<const InstanceSymbol*> children;
        collectInstances(this->instSymbol->body, children);

        // Name cells by their path relative to this instance, so instances
        // in generate blocks and arrays read as "g_loop[0].u_gen" or "u[1]".
        const string prefix = hierPath(*this->instSymbol) + ".";
        for (const InstanceSymbol* instSymb: children) {
            string path = hierPath(*instSymb);
            if (path.starts_with(prefix)) path.erase(0, prefix.size());

            auto cell = make_unique<Cell>(instSymb, path);

            // Generate a unique handle for this instance
            std::string handleStr = "cell" + std::to_string(cellCounter++);

            // Store the instance in the map
            cells[handleStr] = std::move(cell);

            cellHandles.push_back(handleStr);
        }

        return cellHandles;
    }

};

class Cell : public Instance {
public:

    explicit Cell(const InstanceSymbol* instSymbol, string name) : Instance(instSymbol, std::move(name)) {}

    vector<string> getPortConns() {
        vector<string> connHandles;
        for (const PortConnection* pc : instSymbol->getPortConnections()) {
            auto port_connection = make_unique<PortConn>(pc);

            // Generate a unique handle
            string handleStr = "conn" + std::to_string(connCounter++);

            // Store the instance in the map
            connections[handleStr] = std::move(port_connection);

            connHandles.push_back(handleStr);
        }

        return connHandles;
    }

};

class Module : public Instance {
public:
    explicit Module(const InstanceSymbol* instSymbol) : Instance(instSymbol) {}
};

class Tree {
public:

    shared_ptr<SyntaxTree> tree;
    // Owns every Symbol that Module/Port/Cell/Driver handles point into, so it
    // must live as long as the Tree.
    unique_ptr<slang::ast::Compilation> compilation;

    // slang's rendering of every parse and elaboration diagnostic.
    string diagnostics;
    int numErrors = 0;

    explicit Tree(const shared_ptr<SyntaxTree> tree) {
        this->tree = tree;
        compilation = make_unique<slang::ast::Compilation>();
        compilation->addSyntaxTree(tree);

        // Forces full elaboration and collects everything slang reports.
        slang::DiagnosticEngine engine(tree->sourceManager());
        auto client = make_shared<slang::TextDiagnosticClient>();
        engine.addClient(client);
        for (const auto& diag : compilation->getAllDiagnostics()) {
            engine.issue(diag);
        }
        diagnostics = client->getString();
        numErrors = engine.getNumErrors();
    }

    optional<string> getModule(const string& moduleName) const {

        const auto& root = compilation->getRoot();

        // find topmost module instance
        const Symbol *myModuleFind = root.find(moduleName);
        if (myModuleFind == nullptr || myModuleFind->kind != SymbolKind::Instance) {
            return nullopt;
        }

        const auto& myModule = myModuleFind->as<InstanceSymbol>();


        auto mod = make_unique<Module>(&myModule);

        // Generate a unique handle for this instance
        static int modCounter = 0;
        std::string handleStr = "mod" + std::to_string(modCounter++);

        // Store the instance in the map
        modules[handleStr] = std::move(mod);

        return handleStr.c_str();
    };

};


#endif //TCLSLANG_HDL_TREE_H
