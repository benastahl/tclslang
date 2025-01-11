//
// Created by ec2-user on 12/29/24.
//

#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxNode.h"
#include <slang/ast/symbols/PortSymbols.h>
#include <slang/ast/types/DeclaredType.h>
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
class Port;
class Module;
class Tree;

unordered_map<std::string, std::unique_ptr<Port>> ports;
unordered_map<std::string, std::unique_ptr<Module>> modules;
unordered_map<std::string, std::unique_ptr<Tree>> trees;

class Port {
public:

    const PortSymbol*  port;
    string portType; // var or net
    string direction;
    string decType;
    string dimType;
    int startDim;
    int endDim;

    explicit Port(const PortSymbol* port, string portType, string decType, string direction, string dimType, int startDim, int endDim) {
        this->port = port;
        this->portType = std::move(portType);
        this->direction = std::move(direction);
        this->decType = std::move(decType);
        this->startDim = startDim;
        this->endDim = endDim;
        this->dimType = std::move(dimType);
    }

};

class Module {
public:
    string name;
    const InstanceSymbol* moduleDec = nullptr;

    explicit Module(const string& name, const InstanceSymbol* moduleDec) {
        this->name = name;
        this->moduleDec = moduleDec;
    }

    string getHeaderName() const {
        return string(this->moduleDec->name);
    }

    vector<string> getPorts() const {



        vector<string> portHandles;

        // Inspect ports
        for (const auto& portOpt : moduleDec->body.getPortList()) {
            string name;
            string direction;
            string portType;
            string decType;
            string dimType;
            int dimStart = 0;
            int dimEnd = 0;

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
                std::cout << "Port " << portSymbol.name << " is a wire." << std::endl;
            } else {
                decType = "reg";
                std::cout << "Port " << portSymbol.name << " is a reg." << std::endl;
            }

            cout << portSymbol.internalSymbol << endl;
            cout << "kind: " << portSymbol.internalSymbol->kind << endl;

            // Get the port's type and check if it's an array
            const Type& portTypeNode = portSymbol.getType();
            if (portTypeNode.isArray()) {
                const auto &arrayType = portTypeNode.getCanonicalType();
                if (arrayType.isPackedArray()) {
                    const auto &dim = arrayType.as<PackedArrayType>();
                    dimType = "Packed";
                    dimStart = dim.range.left;
                    dimEnd = dim.range.right;
                } else if (arrayType.isUnpackedArray()) {
                    const auto &dim = arrayType.as<FixedSizeUnpackedArrayType>();
                    dimType = "Fixed Size Unpacked";
                    dimStart = dim.range.left;
                    dimEnd = dim.range.right;
                } else if (arrayType.isAssociativeArray()) {
                    const auto &dim = arrayType.as<AssociativeArrayType>();
                    dimType = "Associative";
                    cout << "found 'associative' array type. no dimensions." << endl;
                } else if (arrayType.isDynamicallySizedArray()) {
                    const auto &dim = arrayType.as<DynamicArrayType>();
                    dimType = "Dynamic";
                    cout << "found 'dynamic' array type. no dimensions." << endl;
                } else {
                    cout << "unknown dimension type found." << endl;
                    dimType = "Unknown";
                }
            } else {
                cout << "did not find dimensions for " << name << endl;
            }

            cout << "name: " << name << endl;
            cout << "direction: " << direction << endl;
            cout << "declared type: " << decType << endl;
            cout << "port type: " << portType << endl;
            cout << "dimension type: " << dimType << endl;
            cout << "dimensions: [" << dimStart << ":" << dimEnd << "]" << endl;

            // const PortSymbol* port, string portType, string decType, string direction, int startDim, int endDim
            auto port = make_unique<Port>(&portSymbol, portType, decType, direction, dimType, dimStart, dimEnd);

            // Generate a unique handle
            static int portCounter = 0;
            std::string handleStr = "port" + std::to_string(portCounter++);

            // Store the instance in the map
            ports[handleStr] = std::move(port);

            portHandles.push_back(handleStr);

        }

        return portHandles;
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
        const auto& myModule = root.find<slang::ast::InstanceSymbol>(moduleName);

        // Check if the symbol exists and is an InstanceSymbol
        if (!myModule.isModule()) {
            cout << "Failed to find module with given name." << endl;
            return nullopt;
        }

        auto portList = myModule.body.getPortList();
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
            return TCL_ERROR;
        }
        cout << "done2" << endl;

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
            cout << "got port handle: " << portHandle << endl;
            cout << "port type: " << ports[portHandle]->portType << endl;
            portsObjForm[i] = Tcl_NewStringObj(portHandle.c_str(), portHandle.length());

            Tcl_CreateCommand(interp, portHandle.c_str(), Port_MethodCmd, (ClientData)&ports[portHandle], nullptr);

        }



        Tcl_SetObjResult(interp, Tcl_NewListObj(ports_size, portsObjForm));
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

    bool data_method = (method == "portType" || method == "direction" || method == "type" || method == "portType" || method == "dimType" || method == "startDim" || method == "endDim" || method == "name");

    if (data_method) {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <handle> <data_member>"), TCL_STATIC);
            return TCL_ERROR;
        }

        if (method == "portType") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->portType.c_str(), port->portType.length()));
        } else if (method == "direction") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->direction.c_str(), port->direction.length()));
        } else if (method == "startDim") {
            Tcl_SetObjResult(interp, Tcl_NewIntObj(port->startDim));
        } else if (method == "endDim") {
            Tcl_SetObjResult(interp, Tcl_NewIntObj(port->endDim));
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

// Initialization function required by Tcl
extern "C" int Tclslang_Init(Tcl_Interp* interp) {
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
