//
// Created by ec2-user on 12/29/24.
//

#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxNode.h"
#include <slang/ast/symbols/PortSymbols.h>
#include <slang/ast/types/Type.h>
#include <slang/ast/types/AllTypes.h>
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/Compilation.h>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <iostream>
#include <cstring>
#include <string>
#include <utility>
#include <tcl.h>
#include <slang/syntax/AllSyntax.h>

using namespace slang::syntax;
using namespace slang::ast;
using namespace std;

int Tree_MethodCmd(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[]);
int Module_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
int Port_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
int Cell_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
int Pin_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
void printInstanceTree(const InstanceSymbol* rootInst, int depth);
class Instance;
class Port;
class Module;
class Tree;
class Cell;
class Pin;

unordered_map<string, unique_ptr<Port>> ports;
unordered_map<string, unique_ptr<Module>> modules;
unordered_map<string, unique_ptr<Tree>> trees;
unordered_map<string, unique_ptr<Cell>> cells;
unordered_map<string, unique_ptr<Pin>> pins;

static int cellCounter = 0;
static int portCounter = 0;
static int pinCounter = 0;

/*
module foo ;

instance_type1 instance_name1 ( .pin_name1(net_name1), .pin_name2(net_name2) ) ;
instance_type2 instance_name2 ( .pin_name3(net_name3), .pine_name4(net_name4) );

endmodule

instance_inst => {name ports port connections}

mod get_ports -> {port_inst port2_inst}

mod get_cells -> {instance1_inst instance2_inst}
inst get_pins -> {pin_inst1 pin_inst2}
pin get_nets  -> {net_name1}
*/

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

class Pin {
public:
    const PortConnection* pin = nullptr;
    bool netIsPort = false;

    explicit Pin(const PortConnection* pin) {
        this->pin = pin;
    }

    // returns either port inst, constant (3, 4), or signal name (if not constant, or symbol).
    void getNet(string* netStrPtr) {
        cout << "[Note] Getting net expression...\n";

        // gets net expression
        const Expression* expr = this->pin->getExpression();

        // checking for valid expression.
        if (!expr) {
            cout << "[Warning] No net expression connected to port.\n";
            return;
        } else if (expr->bad()) {
            cout << "[Error] Net expression is invalid or has syntax errors\n";
            return;
        }

        const auto& constant = expr->constant;  // checks if net is a constant value (int, string, byte, etc).

        if (constant) {
            *netStrPtr = constant->toString();  // sets net to constant as string.
            return;
        }

        cout << "[Note] Net expression is not a constant.\n";

        // if expr (symbol) is not a port, just returns the name of the symbol (likely signal name).
        const Symbol* symb = expr->getSymbolReference();
        if (symb->kind != SymbolKind::Port) {
            cout << "[Note] Net expression is not a port.\n";
            *netStrPtr = symb->name;
            return;
        }

        // net expr is a port. create port instance to use in tcl and return handle.
        cout << "[Note] Net expression is a port.\n";
        this->netIsPort = true;
        const auto& portSymb = symb->as<PortSymbol>();

        string portHandle;
        addPort(&portSymb, &portHandle);  // adds port handle to tcl commands

        *netStrPtr = portHandle;  // set net string to port handle tcl command
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

    vector<string> getPins() {
        vector<string> pinHandles;
        for (const PortConnection* pc : instSymbol->getPortConnections()) {
            cout << "got port " << pc->port.name << "of kind " << toString(pc->port.kind) << " connected to " << \
             pc->getExpression() << " of kind " << toString(pc->getExpression()->kind) << endl;

            auto pin = make_unique<Pin>(pc);

            // Generate a unique handle
            string handleStr = "pin" + std::to_string(pinCounter++);

            // Store the instance in the map
            pins[handleStr] = std::move(pin);

            pinHandles.push_back(handleStr);
        }

        return pinHandles;
    }

};

class Module : public Instance {
public:
    explicit Module(const InstanceSymbol* instSymbol) : Instance(instSymbol) {}
};

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
            cout << port->name << " ";
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

int SlangParse(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]) {
    if (argc < 2) {
        Tcl_SetResult(interp, const_cast<char*>("Usage: slang_parse [<verilog_file> ...]"), TCL_STATIC);
        return TCL_ERROR;
    }

    vector<string_view> filePaths;
    for (int i = 1; i < argc; i++) {
        filePaths.emplace_back(argv[i]);
    }

    auto syntaxTreeOpt = SyntaxTree::fromFiles(filePaths);
    if (!syntaxTreeOpt) {
        Tcl_SetResult(interp, const_cast<char*>("Failed to parse Verilog file."), TCL_VOLATILE);
        return TCL_ERROR;
    }

    const auto& syntaxTree = syntaxTreeOpt.value();

    // Create a new Tree instance
    auto tree = std::make_unique<Tree>(syntaxTree);

    // Generate a unique handle
    static int treeCounter = 0;
    std::string handleStr = "tree" + std::to_string(treeCounter++);

    // Store the instance in the map
    trees[handleStr] = std::move(tree);

    // Register a new Tcl command using the handle
    Tcl_CreateCommand(interp, handleStr.c_str(), Tree_MethodCmd, (ClientData)&trees[handleStr], nullptr);

    // Return the handle to Tcl
    Tcl_SetObjResult(interp, Tcl_NewStringObj(handleStr.c_str(), handleStr.length()));

    return TCL_OK;
}

int Tree_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]) {
    if (argc < 2) {
        Tcl_SetResult(interp, const_cast<char*>("Usage: <handle> <method> [args...]"), TCL_STATIC);
        return TCL_ERROR;
    }

    // Get the handle name (e.g., "tree0")
    std::string handle = argv[0];

    // Find the corresponding Tree instance
    auto it = trees.find(handle);
    if (it == trees.end()) {
        Tcl_SetResult(interp, const_cast<char*>("Invalid tree handle"), TCL_STATIC);
        return TCL_ERROR;
    }

    auto& tree = it->second;

    std::string method = argv[1];
    if (method == "get_module") {
        if (argc != 3) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <handle> get_module <module_name>"), TCL_STATIC);
            return TCL_ERROR;
        }

        std::string moduleName = argv[2];
        auto moduleHandle = tree->getModule(moduleName);
        if (moduleHandle == nullopt) {
            Tcl_SetResult(interp, const_cast<char*>("Module not found"), TCL_STATIC);
            return TCL_OK;
        }

        // set module method command
        Tcl_CreateCommand(interp, moduleHandle.value().c_str(), Module_MethodCmd, (ClientData)&modules[moduleHandle.value()], nullptr);

        Tcl_SetObjResult(interp, Tcl_NewStringObj(moduleHandle->c_str(), moduleHandle->length()));
        return TCL_OK;
    }

    Tcl_SetResult(interp, const_cast<char*>("Unknown method"), TCL_STATIC);
    return TCL_ERROR;
}

int Module_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]) {
    if (argc < 2) {
        Tcl_SetResult(interp, const_cast<char*>("Usage: <module_name> <method> [args...]"), TCL_STATIC);
        return TCL_ERROR;
    }

    // Get the handle name (e.g., "module0")
    std::string handle = argv[0];

    // Find the corresponding Module instance
    auto it = modules.find(handle);
    if (it == modules.end()) {
        Tcl_SetResult(interp, const_cast<char*>("Invalid module handle"), TCL_STATIC);
        return TCL_ERROR;
    }

    auto& module = it->second;

    // handle method called
    std::string method = argv[1];
    if (method == "name") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <module_name> name"), TCL_STATIC);
            return TCL_ERROR;
        }

        Tcl_SetObjResult(interp, Tcl_NewStringObj(module->name.c_str(), module->name.length()));
        return TCL_OK;
    } else if (method == "get_ports") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <module_name> get_ports"), TCL_STATIC);
            return TCL_ERROR;
        }

        // collect ports
        vector<string> portHandles = module->getPorts();

        // convert ports to Tcl_StringObj
        const int ports_size = portHandles.size();
        Tcl_Obj* portsObjForm[ports_size];
        for (int i=0; i < ports_size; i++) {
            const string portHandle = portHandles[i];
            portsObjForm[i] = Tcl_NewStringObj(portHandle.c_str(), portHandle.length());

            Tcl_CreateCommand(interp, portHandle.c_str(), Port_MethodCmd, (ClientData)&ports[portHandle], nullptr);
        }

        Tcl_SetObjResult(interp, Tcl_NewListObj(ports_size, portsObjForm));
        return TCL_OK;
    } else if (method == "get_cells") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <module_name> get_cells"), TCL_STATIC);
            return TCL_ERROR;
        }

        // collect cells
        vector<string> cellHandles = module->getCells();

        // convert cells to Tcl_StringObj
        const unsigned int cells_size = cellHandles.size();
        Tcl_Obj* cellsObjForm[cells_size];
        for (int i=0; i < cells_size; i++) {
            const string cellHandle = cellHandles[i];
            cellsObjForm[i] = Tcl_NewStringObj(cellHandle.c_str(), cellHandle.length());

            Tcl_CreateCommand(interp, cellHandle.c_str(), Cell_MethodCmd, (ClientData)&cells[cellHandle], nullptr);
        }

        Tcl_SetObjResult(interp, Tcl_NewListObj(cells_size, cellsObjForm));
        return TCL_OK;
    }

    Tcl_SetResult(interp, const_cast<char*>("Unknown method"), TCL_STATIC);
    return TCL_ERROR;
}

int Port_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]) {
    if (argc < 2) {
        Tcl_SetResult(interp, const_cast<char*>("Usage: <handle> <method> [args...]"), TCL_STATIC);
        return TCL_ERROR;
    }

    // Get the handle name (e.g., "port0")
    std::string handle = argv[0];

    // Find the corresponding Tree instance
    auto it = ports.find(handle);
    if (it == ports.end()) {
        Tcl_SetResult(interp, const_cast<char*>("Invalid port handle"), TCL_STATIC);
        return TCL_ERROR;
    }

    auto& port = it->second;

    std::string method = argv[1];

    bool data_method = (method == "portType" || method == "direction" || method == "type" || method == "portType" || method == "dimType" || method == "dimensions" || method == "name");

    if (data_method) {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <handle> <data_member>"), TCL_STATIC);
            return TCL_ERROR;
        }

        if (method == "portType") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->portType.c_str(), port->portType.length()));
        } else if (method == "direction") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->direction.c_str(), port->direction.length()));
        } else if (method == "dimensions") {
            const unsigned int dimCount = (const unsigned int)port->dimensions.size();

            Tcl_Obj* dims[dimCount];

            unsigned int nDim = 0;
            for (array<int, 2> dim : port->dimensions) {
                Tcl_Obj* dimRange[2] = {Tcl_NewIntObj(dim[0]), Tcl_NewIntObj(dim[1])};

                Tcl_Obj* obj = Tcl_NewListObj(2, dimRange);
                dims[nDim] = obj;
                nDim++;
            }


            Tcl_SetObjResult(interp, Tcl_NewListObj(dimCount, dims));
        } else if (method == "name") {
            string portString = string(port->port->name);
            Tcl_SetObjResult(interp, Tcl_NewStringObj(portString.c_str(), portString.length()));
        } else if (method == "type") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->decType.c_str(), port->decType.length()));
        } else if (method == "dimType") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->dimType.c_str(), port->dimType.length()));
        } else {
            Tcl_SetResult(interp, const_cast<char*>("Invalid data member name used."), TCL_STATIC);
            return TCL_ERROR;
        }

        return TCL_OK;
    }

    Tcl_SetResult(interp, const_cast<char*>("Unknown method"), TCL_STATIC);
    return TCL_ERROR;
}

int Cell_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]) {
    if (argc < 2) {
        Tcl_SetResult(interp, const_cast<char *>("Usage: <cell_name> <method> [args...]"), TCL_STATIC);
        return TCL_ERROR;
    }

    // Get the handle name (e.g., "module0")
    std::string handle = argv[0];

    // Find the corresponding Module instance
    auto it = cells.find(handle);
    if (it == cells.end()) {
        Tcl_SetResult(interp, const_cast<char *>("Invalid cell handle"), TCL_STATIC);
        return TCL_ERROR;
    }

    auto &cell = it->second;

    // handle method called
    std::string method = argv[1];
    if (method == "name") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <cell_name> name"), TCL_STATIC);
            return TCL_ERROR;
        }

        Tcl_SetObjResult(interp, Tcl_NewStringObj(cell->name.c_str(), cell->name.length()));
        return TCL_OK;
    } else if (method == "get_ports") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <cell_name> get_ports"), TCL_STATIC);
            return TCL_ERROR;
        }

        // collect ports
        vector<string> portHandles = cell->getPorts();

        // convert ports to Tcl_StringObj
        const int ports_size = portHandles.size();
        Tcl_Obj* portsObjForm[ports_size];
        for (int i=0; i < ports_size; i++) {
            const string portHandle = portHandles[i];
            portsObjForm[i] = Tcl_NewStringObj(portHandle.c_str(), portHandle.length());

            Tcl_CreateCommand(interp, portHandle.c_str(), Port_MethodCmd, (ClientData)&ports[portHandle], nullptr);
        }

        Tcl_SetObjResult(interp, Tcl_NewListObj(ports_size, portsObjForm));
        return TCL_OK;
    } else if (method == "get_pins") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <cell_name> get_pins"), TCL_STATIC);
            return TCL_ERROR;
        }

        // collect cells
        vector<string> pinHandles = cell->getPins();

        // convert cells to Tcl_StringObj
        const unsigned int pins_size = pinHandles.size();
        Tcl_Obj* cellsObjForm[pins_size];
        for (int i=0; i < pins_size; i++) {
            const string pinHandle = pinHandles[i];
            cellsObjForm[i] = Tcl_NewStringObj(pinHandle.c_str(), pinHandle.length());

            Tcl_CreateCommand(interp, pinHandle.c_str(), Pin_MethodCmd, (ClientData)&cells[pinHandle], nullptr);
        }

        Tcl_SetObjResult(interp, Tcl_NewListObj(pins_size, cellsObjForm));
        return TCL_OK;
    } else if (method == "get_cells") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <cell_name> get_cells"), TCL_STATIC);
            return TCL_ERROR;
        }

        // collect cells
        vector<string> cellHandles = cell->getCells();

        // convert cells to Tcl_StringObj
        const unsigned int cells_size = cellHandles.size();
        Tcl_Obj* cellsObjForm[cells_size];
        for (int i=0; i < cells_size; i++) {
            const string cellHandle = cellHandles[i];
            cellsObjForm[i] = Tcl_NewStringObj(cellHandle.c_str(), cellHandle.length());

            Tcl_CreateCommand(interp, cellHandle.c_str(), Cell_MethodCmd, (ClientData)&cells[cellHandle], nullptr);
        }

        Tcl_SetObjResult(interp, Tcl_NewListObj(cells_size, cellsObjForm));
        return TCL_OK;
    }

    Tcl_SetResult(interp, const_cast<char*>("Unknown method"), TCL_STATIC);
    return TCL_ERROR;
}

int Pin_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]) {
    if (argc < 2) {
        Tcl_SetResult(interp, const_cast<char *>("Usage: <pin_name> <method> [args...]"), TCL_STATIC);
        return TCL_ERROR;
    }

    // Get the handle name (e.g., "module0")
    std::string handle = argv[0];

    // Find the corresponding Module instance
    auto it = pins.find(handle);
    if (it == pins.end()) {
        Tcl_SetResult(interp, const_cast<char *>("Invalid pin handle"), TCL_STATIC);
        return TCL_ERROR;
    }

    auto &pin = it->second;

    // handle method called
    std::string method = argv[1];
    if (method == "name") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <pin_name> name"), TCL_STATIC);
            return TCL_ERROR;
        }

        Tcl_SetObjResult(interp, Tcl_NewStringObj(string(pin->pin->port.name).c_str(), pin->pin->port.name.size()));
        return TCL_OK;
    } else if (method == "get_net") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <pin_name> get_net"), TCL_STATIC);
            return TCL_ERROR;
        }

        string net; // can either be constant, signal name, or port instance handle
        pin->getNet(&net);

        if (pin->netIsPort) {
            Tcl_CreateCommand(interp, net.c_str(), Port_MethodCmd, (ClientData)&pins[net], nullptr);
        }

        Tcl_SetObjResult(interp, Tcl_NewStringObj(net.c_str(), net.length()));
        return TCL_OK;
    } else if (method == "is_port") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <pin_name> is_port"), TCL_STATIC);
            return TCL_ERROR;
        }

        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(pin->netIsPort));
        return TCL_OK;
    }

    Tcl_SetResult(interp, const_cast<char*>("Unknown method"), TCL_STATIC);
    return TCL_ERROR;
}

// Initialization function required by Tcl
extern "C" [[maybe_unused]] int Tclslang_Init(Tcl_Interp* interp) {
    // Check Tcl version compatibility
    if (Tcl_InitStubs(interp, "8.1", 0) == nullptr) {
        return TCL_ERROR;
    }

    // Register the C++ functions as Tcl commands
    Tcl_CreateCommand(interp, "slang_parse", SlangParse, nullptr, nullptr);

    // Provide the package
    Tcl_PkgProvide(interp, "tclslang", "1.0");

    return TCL_OK;
}
