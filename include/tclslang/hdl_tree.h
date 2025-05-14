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
class Variable;
class Net;
class Constant;
class Driver;

unordered_map<string, unique_ptr<Port>>     ports;
unordered_map<string, unique_ptr<Module>>   modules;
unordered_map<string, unique_ptr<Tree>>     trees;
unordered_map<string, unique_ptr<Cell>>     cells;
unordered_map<string, unique_ptr<PortConn>> connections;

/* testing */
unordered_map<string, unique_ptr<Driver>>   drivers;

unordered_map<string, unique_ptr<Variable>>     variables;
unordered_map<string, unique_ptr<Net>>          nets;
unordered_map<string, unique_ptr<Constant>>     constants;


/**/

static int cellCounter  = 0;
static int portCounter  = 0;
static int connCounter  = 0;
static int driverCounter = 0;


// Just adds port to list of port instances for use in tcl.
void addPort(const PortSymbol* portSymbol, string* handleStr) {

    const Symbol *internal = portSymbol->internalSymbol;
    if (!internal) {
        std::cout << "Internal symbol is null" << std::endl;
        return;
    }

    // const PortSymbol* port, string portType, string decType, string direction, int startDim, int endDim
    auto port = make_unique<Port>(portSymbol);

    // Generate a unique handle
    *handleStr = "port" + std::to_string(portCounter++);

    // Store the instance in the map
    ports[*handleStr] = std::move(port);
}

void printInstanceTree(const InstanceSymbol* rootInst, int depth = 1) {
    std::string indent(depth, '\t');

    if (depth == 1) {
        cout << "InstanceSymbol:"
             << "\n" << "\tName: " << rootInst->name
             << "\n" << "\tKind: " << rootInst->kind;

        cout << "\n" << "\tPort List: ";
        for (const Symbol* port : rootInst->body.getPortList()) {
            cout << port->name << " ";
        }
        cout << "\n";

        cout << "\tPort Connections:";
        for (const PortConnection* portConn : rootInst->getPortConnections()) {
            cout << " (" << portConn->port.name << " -> ";
            if (auto* expr = portConn->getExpression()) {
                if (auto* symRef = expr->getSymbolReference()) {
                    cout << symRef->name;
                } else {
                    cout << "unresolved";
                }
            } else {
                cout << "unconnected";
            }
            cout << ") ";
        }
        cout << "\n";
    }

    for (const InstanceSymbol& inst : rootInst->body.membersOfType<InstanceSymbol>()) {
        cout << indent << "InstanceSymbol:"
             << "\n" << indent << "\tName: " << inst.name
             << "\n" << indent << "\tKind: " << inst.kind;

        cout << "\n" << indent << "\tPort List: ";
        for (const Symbol* port : inst.body.getPortList()) {
            if (port->kind == SymbolKind::Port) {
                cout << port->name << " ";
            }
        }
        cout << "\n";

        cout << indent << "\tPort Connections:";
        for (const PortConnection* portConn : inst.getPortConnections()) {
            cout << " (" << portConn->port.name << " -> ";
            if (auto* expr = portConn->getExpression()) {
                if (auto* symRef = expr->getSymbolReference()) {
                    cout << symRef->name;
                } else {
                    cout << "unresolved";
                }
            } else {
                cout << "unconnected";
            }
            cout << ") ";
        }
        cout << "\n";

        printInstanceTree(&inst, depth + 1);
    }
}

// driver is the destination of a port connection. can be of type variable or net
class Driver {
public:
    string type;
    const Symbol* driverSymbol;
    string constant;

    explicit Driver(const Symbol* driverSymbol, string type, string constant = "") {
        this->type = std::move(type);
        this->driverSymbol = driverSymbol;
        this->constant = std::move(constant);
    }

};


class PortConn {
public:
    const PortConnection* portConn = nullptr;
    const PortSymbol* portSymbol = nullptr;
    const Port* port = nullptr;
    const Driver* driver = nullptr;
    string portHandle;

    explicit PortConn(const PortConnection* portConn) {
        this->portConn = portConn;
        this->portSymbol = &portConn->port.as<PortSymbol>();

        addPort(this->portSymbol, &portHandle); // adds port to port handles list for use in tcl
        this->port = ports[portHandle].get(); // sets this->port to a Port class for use

        setDriver(); // sets this->driver to value
    }

    void setDriver() {
        cout << "[Note] Getting driver...\n";
        const Expression* expr = this->portConn->getExpression();

        // checking for valid expression.
        if (!expr) {
            cout << "[Warning] No driver expression connected to port.\n";
            return;
        } else if (expr->bad()) {
            cout << "[Error] driver expression is invalid or has syntax errors\n";
            return;
        }

        const auto& constant = expr->constant;  // checks if driver is a constant value (int, string, byte, etc).

        string handleStr;
        unique_ptr<Driver> driver;

        // driver can either be constant, net symbol, or variable symbol.

        if (constant) {
            driver = make_unique<Driver>(nullptr, "const", constant->toString());

            handleStr = "driver" + std::to_string(driverCounter++);
            drivers[handleStr] = std::move(driver);     // move the unique_ptr into the map
            this->driver = drivers[handleStr].get();        // get the raw pointer from the map

            return;
        }

        const Symbol* symb = expr->getSymbolReference();
        if (symb->kind == SymbolKind::Net) {
            driver = make_unique<Driver>(symb, "net");
        } else if (symb->kind == SymbolKind::Variable) {
            driver = make_unique<Driver>(symb, "var");
        }

        handleStr = "driver" + std::to_string(driverCounter++);
        drivers[handleStr] = std::move(driver);     // move the unique_ptr into the map
        this->driver = drivers[handleStr].get();        // get the raw pointer from the map


    }

};

class Port {
public:
    const PortSymbol *port;
    string portType; // var or net
    string direction;
    string decType;
    string dimType;
    vector<array<int, 2>> dimensions;

    explicit Port(const PortSymbol *portSymbol) {
        this->port = portSymbol;

        cout << "Port Name: " << portSymbol->name << endl;

        this->direction = toString(portSymbol->direction);
        this->portType = toString(portSymbol->internalSymbol->kind);

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
                cout << "  Packed array range: " << packedArray.range.left << " to " << packedArray.range.right
                     << endl;
                this->dimensions.push_back({packedArray.range.left, packedArray.range.right});

                type = &packedArray.elementType;
            } else if (type->isUnpackedArray()) {
                const auto &unpackedArray = type->getCanonicalType().as<FixedSizeUnpackedArrayType>();
                cout << "  Unpacked array range: " << unpackedArray.range.left << " to "
                     << unpackedArray.range.right << endl;
                this->dimensions.push_back({unpackedArray.range.left, unpackedArray.range.right});

                type = &unpackedArray.elementType;
            } else {
                cout << "  Unknown array type." << endl;
                break;
            }
        }
    }

};

class Instance {
public:
    string name;
    const InstanceSymbol* instSymbol = nullptr;

    explicit Instance(const InstanceSymbol* instSymbol) {
        this->instSymbol = instSymbol;
        this->name = string(this->instSymbol->name);
    }

    vector<string> getPorts() const {
        vector<string> portHandles;

        for (const auto &portOpt: this->instSymbol->body.getPortList()) {
            const auto &portSymbol = portOpt->as<PortSymbol>();

            string portHandle;
            addPort(&portSymbol, &portHandle);

            if (portHandle.empty()) continue;

            portHandles.push_back(portHandle);
        }

        return portHandles;
    }

    vector<string> getCells() const {
        vector<string> cellHandles;

        for (const InstanceSymbol &instSymb: this->instSymbol->body.membersOfType<InstanceSymbol>()) {
            auto cell = make_unique<Cell>(&instSymb);

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

    explicit Cell(const InstanceSymbol* instSymbol) : Instance(instSymbol) {}

    vector<string> getPortConns() {
        vector<string> connHandles;
        for (const PortConnection* pc : instSymbol->getPortConnections()) {
            cout << "got port " << pc->port.name << " of kind " << toString(pc->port.kind) << "\n";
            auto port_connection = make_unique<PortConn>(pc);

//            const auto& net_thing = pc->getExpression()->getSymbolReference()->as<VariableSymbol>();

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

    explicit Tree(const shared_ptr<SyntaxTree> tree) {
        this->tree = tree;
    }

    optional<string> getModule(const string& moduleName) const {

        slang::ast::Compilation compilation;
        compilation.addSyntaxTree(tree);
        const auto& root = compilation.getRoot();

        // find topmost module instance
        const Symbol *myModuleFind = root.find(moduleName);
        if (myModuleFind == nullptr) {
            cout << "Failed to find module with given name." << endl;
            return nullopt;
        }

        const auto& myModule = myModuleFind->as<InstanceSymbol>();

        printInstanceTree(&myModule);

        cout << "got instance: " << myModule.name << " of type " << toString(myModule.kind) << endl;

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
