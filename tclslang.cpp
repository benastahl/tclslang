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
class Port;
class Module;
class Tree;
class Cell;

unordered_map<std::string, std::unique_ptr<Port>> ports;
unordered_map<std::string, std::unique_ptr<Module>> modules;
unordered_map<std::string, std::unique_ptr<Tree>> trees;
unordered_map<std::string, std::unique_ptr<Cell>> cells;

class Cell {

};

class Port {
public:

    const PortSymbol*  port;
    string portType; // var or net
    string direction;
    string decType;
    string dimType;
    vector<array<int, 2>> dimensions;

    explicit Port(const PortSymbol* port, string portType, string decType, string direction, string dimType, vector<array<int, 2>> dimensions) {
        this->port = port;
        this->portType = std::move(portType);
        this->direction = std::move(direction);
        this->decType = std::move(decType);
        this->dimensions = std::move(dimensions);
        this->dimType = std::move(dimType);
    }

};

class Module {
public:
    string name;
    const InstanceSymbol* moduleInstanceSymbol = nullptr;

    explicit Module(const string& name, const InstanceSymbol* moduleInstanceSymbol) {
        this->name = name;
        this->moduleInstanceSymbol = moduleInstanceSymbol;
    }

    string getHeaderName() const {
        return string(this->moduleInstanceSymbol->name);
    }

    vector<string> getPorts() const {
        vector<string> portHandles;

        // Inspect ports
        for (const auto& portOpt : moduleInstanceSymbol->body.getPortList()) {
            string name;
            string direction;
            string portType;
            string decType;
            string dimType;
            vector<array<int, 2>> dimensions;

            const auto& portSymbol = portOpt->as<PortSymbol>();

            name = portSymbol.name;
            direction = toString(portSymbol.direction);
            portType = toString(portSymbol.internalSymbol->kind);

            const Symbol* internal = portSymbol.internalSymbol;
            if (!internal) {
                std::cout << "Internal symbol is null" << std::endl;
                continue;
            }

            // Determine if it's wire or reg
            if (portSymbol.isNetPort() || portSymbol.direction == ArgumentDirection::In || portSymbol.direction == ArgumentDirection::InOut) {
                decType = "wire";
            } else {
                decType = "reg";
            }

            // Get the port's type and check if it's an array
            const Type* type = &portSymbol.getType();

            while (type->isArray()) {
                if (type->isPackedArray()) {
                    const auto& packedArray = type->getCanonicalType().as<PackedArrayType>();
                    cout << "  Packed array range: " << packedArray.range.left << " to " << packedArray.range.right << endl;
                    dimensions.push_back({packedArray.range.left, packedArray.range.right});

                    type = &packedArray.elementType;
                } else if (type->isUnpackedArray()) {
                    const auto& unpackedArray = type->getCanonicalType().as<FixedSizeUnpackedArrayType>();
                    cout << "  Unpacked array range: " << unpackedArray.range.left << " to " << unpackedArray.range.right << endl;
                    dimensions.push_back({unpackedArray.range.left, unpackedArray.range.right});

                    type = &unpackedArray.elementType;
                } else {
                    cout << "  Unknown array type." << endl;
                    break;
                }
            }

            // const PortSymbol* port, string portType, string decType, string direction, int startDim, int endDim
            auto port = make_unique<Port>(&portSymbol, portType, decType, direction, dimType, dimensions);

            // Generate a unique handle
            static int portCounter = 0;
            std::string handleStr = "port" + std::to_string(portCounter++);

            // Store the instance in the map
            ports[handleStr] = std::move(port);

            portHandles.push_back(handleStr);

        }

        return portHandles;
    }

    vector<string> getCells() const {
        vector<string> cellHandles;
        cout << moduleInstanceSymbol->name << endl;
        if (moduleInstanceSymbol->getPortConnections().empty()) {
            std::cout << "No port connections found for instance: " << moduleInstanceSymbol->name << std::endl;
        }

        cout << "got to cells func" << endl;
        for (const auto& portConn : this->moduleInstanceSymbol->getPortConnections()) {
            const auto& port = portConn->port;
            const auto* expr = portConn->getExpression();
            cout << "inside!" << endl;
            std::cout << "Port: " << port.name << " -> ";
            if (expr) {
                std::cout << "Connected to: " << expr << std::endl;
            } else {
                std::cout << "Unconnected" << std::endl;
            }
        }

        return cellHandles;

    }

};

class Tree {
public:

    shared_ptr<SyntaxTree> tree;

    explicit Tree(const shared_ptr<SyntaxTree> tree) {
        this->tree = tree;
    }

    optional<string> getModule(const string& moduleName) const {

        // Use find() without a template argument
        slang::ast::Compilation compilation;
        compilation.addSyntaxTree(tree);
        const auto& root = compilation.getRoot();
        const auto& myModuleFind = root.lookupName<slang::ast::InstanceSymbol>(moduleName);

        // Check if the symbol exists and is an InstanceSymbol
        if (!myModuleFind.isModule()) {
            cout << "Failed to find module with given name." << endl;
            return nullopt;
        }

        const auto& myModule = myModuleFind.as<slang::ast::InstanceSymbol>();

        for (const auto& member : myModule.body.members()) {
            // Check if the member is an Instance
            if (member.kind == slang::ast::SymbolKind::Instance) {
                const auto& instance = member.as<slang::ast::InstanceSymbol>();
                std::cout << "Instance: " << instance.name
                          << ", Type: " << instance.getDefinition().name << "\n";

                for (const auto& portConnection : instance.getPortConnections()) {
                    const auto& port = portConnection->port; // Port symbol
                    const auto* connection = portConnection->getExpression(); // Port connection

                    std::cout << "Port: " << port.name
                              << ", Signal: " << (connection ? connection : "unconnected") << "\n";
                }
            } else {
                // For non-instance members, output their kind for debugging
                std::cout << "'" << member.name << "' is of kind "
                          << slang::ast::toString(member.kind) << "\n";
            }
        }

        auto portList = myModule.body.getPortList();

        auto cellList = myModule.getPortConnections();
        auto mod = make_unique<Module>(moduleName, &myModule);

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

        const string headerName = module->getHeaderName();

        Tcl_SetObjResult(interp, Tcl_NewStringObj(headerName.c_str(), headerName.length()));
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
    if (method == "") {

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
