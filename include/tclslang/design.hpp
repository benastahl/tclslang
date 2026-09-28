// TCLSLANG BY BEN STAHL
// https://github.com/benastahl/tclslang
//
// The elaborated-design model behind the Tcl API.
//
// Every object points into the slang Compilation of the Tree it came from, so
// the Tree owns all of them. Objects form an ownership tree (Tree -> Module ->
// Cell -> Connection -> Driver, ...); destroying an object destroys everything
// below it. Each (kind, parent, slang pointer) maps to exactly one object, so
// asking for the same port twice returns the same object.

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <slang/ast/Compilation.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceManager.h>
#include <tcl.h>

namespace slang::ast {
class Expression;
class InstanceSymbol;
class PortConnection;
class Symbol;
} // namespace slang::ast

namespace tclslang {

class Tree;

class Object {
public:
    enum class Kind { Tree, Module, Cell, Port, Connection, Driver };

    Object(Kind kind, Tree& tree, Object* parent, const void* key) :
        kind(kind), tree(tree), parent(parent), key(key) {}
    virtual ~Object() = default;
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    const Kind kind;
    Tree& tree;
    Object* const parent;       // nullptr for a Tree
    const void* const key;      // slang pointer this object wraps
    std::vector<Object*> children;

    // Set by the Tcl binding when the object is first returned to a script.
    std::string handle;
    Tcl_Command command = nullptr;
};

class Port : public Object {
public:
    Port(Tree& tree, Object* parent, const slang::ast::Symbol& symbol);

    const slang::ast::Symbol& symbol;
    std::string name;
    std::string direction;      // input, output, inout, ref; empty for interface ports
    std::string kind;           // net, var, interface, multiport
    std::string dataType;       // slang's type, e.g. logic[7:0]
    std::string netType;        // wire, tri, ... for net ports
    uint64_t width = 0;         // total bits, including unpacked dimensions
    std::string interfaceName;  // interface ports
    std::string modport;        // interface ports with a modport
    std::vector<std::array<int32_t, 2>> dimensions;
};

// What a port connection is hooked up to. `kind` is one of:
//   const      a constant expression (literal, parameter)
//   var / net  a reference to (a select of) a single variable or net
//   expr       any other expression, e.g. {a, b} or a & b
//   interface  an interface instance or modport on an interface port
class Driver : public Object {
public:
    Driver(Tree& tree, Object* parent, const void* key, std::string kind);

    std::string kind;
    const slang::ast::Symbol* symbol = nullptr;     // var, net, interface
    const slang::ast::Expression* expr = nullptr;   // everything but interface
    std::string constant;                           // const
    std::string modport;                            // interface

    std::string name() const;
    std::string text() const;       // source text of the connection
    std::string dataType() const;
    std::string netType() const;
};

class Connection : public Object {
public:
    Connection(Tree& tree, Object* parent, const slang::ast::PortConnection& conn);

    const slang::ast::PortConnection& conn;

    std::string name() const;
    Port* port();
    Driver* driver();   // nullptr when the port is unconnected
};

// A module (from get_module) or a cell (an instance inside another instance).
class Instance : public Object {
public:
    Instance(Kind kind, Tree& tree, Object* parent, const slang::ast::InstanceSymbol& symbol,
             std::string name, std::string path);

    const slang::ast::InstanceSymbol& symbol;
    std::string name;   // module name for a module; path below the parent for a cell
    std::string path;   // full hierarchical path, e.g. top.u_core.g[0].u_alu

    std::string refName() const;   // the module/interface this instantiates

    std::vector<Port*> ports();
    std::vector<Instance*> cells();
    std::vector<Connection*> connections();
};

class Tree : public Object {
public:
    // Parses and elaborates `paths` as one design. Returns nullptr and sets
    // `error` if a file cannot be read or the design has errors.
    static std::unique_ptr<Tree> parse(const std::vector<std::string>& paths, std::string& error);

    // A top-level instance of module `name`, or else the first instance of it
    // found walking down from the tops. nullptr if the design has none.
    Instance* findModule(std::string_view name);
    std::vector<std::string> topModules() const;
    const std::string& diagnostics() const { return diagText; }
    const slang::SourceManager& sources() const { return sourceManager; }

    // Returns the object for (kind, parent, key), creating it with `make` the
    // first time.
    template<typename T, typename Make>
    T* intern(Kind kind, Object* parent, const void* key, Make&& make) {
        auto& slot = objects[{kind, parent, key}];
        if (!slot) {
            slot = make();
            parent->children.push_back(slot.get());
        }
        return static_cast<T*>(slot.get());
    }

    // Frees `object`, which must have no children left.
    void release(Object* object);

    Tcl_Interp* interp = nullptr;

private:
    Tree();

    // Declared first so it is destroyed last: syntax trees and the
    // compilation refer to the source buffers it owns.
    slang::SourceManager sourceManager;
    std::shared_ptr<slang::syntax::SyntaxTree> syntax;
    std::unique_ptr<slang::ast::Compilation> compilation;
    std::string diagText;
    std::map<std::tuple<Kind, const Object*, const void*>, std::unique_ptr<Object>> objects;
};

} // namespace tclslang
