//
// Created by ec2-user on 12/29/24.
//

#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxNode.h"
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <iostream>
#include <cstring>
#include <string>
#include <utility>
#include <tcl.h>
#include <slang/syntax/AllSyntax.h>

using namespace slang::syntax;
using namespace std;

void getNode(const SyntaxNode& node, SyntaxKind kind, const SyntaxNode*& searchNode);
void getAllNodes(const SyntaxNode& node, SyntaxKind kind, std::vector<const SyntaxNode*>& nodes);
int Tree_MethodCmd(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[]);
int Module_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
void traverseNode(const SyntaxNode& node, int depth);
int Port_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
class Port;
class Module;
class Tree;

unordered_map<std::string, std::unique_ptr<Port>> ports;
unordered_map<std::string, std::unique_ptr<Module>> modules;
unordered_map<std::string, std::unique_ptr<Tree>> trees;

class Port {
public:

    const SyntaxNode*  port;
    string portType; // var or net
    string direction;
    string netType;
    string dataType;
    int startDim;
    int endDim;
    vector<string> identifiers;

    explicit Port(const SyntaxNode* port, string portType, string direction, string netType, string dataType, int startDim, int endDim, vector<string> identifiers) {
        this->port = port;
        this->portType = std::move(portType);
        this->direction = std::move(direction);
        this->netType = std::move(netType);
        this->dataType = std::move(dataType);
        this->startDim = startDim;
        this->endDim = endDim;
        this->identifiers = std::move(identifiers);
    }

    string getString() {
        return port->toString();
    }

};

class Module {
public:
    string name;
    const ModuleDeclarationSyntax* moduleDec = nullptr;

    explicit Module(const string& name, const ModuleDeclarationSyntax* moduleDec) {
        this->name = name;
        this->moduleDec = moduleDec;
    }

    string getHeaderName() const {
        return string(this->moduleDec->header->name.valueText());
    }

    vector<string> getPorts() const {
        vector<const SyntaxNode*> portNodes;

        getAllNodes(*this->moduleDec, SyntaxKind::PortDeclaration, portNodes); // for ports declared after module header
        getAllNodes(*this->moduleDec, SyntaxKind::ImplicitAnsiPort, portNodes);   // for ports declared inside module header

        cout << "found " << portNodes.size() << " ports" << endl;

        vector<string> portStrings;
        for (const SyntaxNode* portDecNode: portNodes) {

            // get port header
            const SyntaxNode* netPortNode = nullptr;
            const SyntaxNode* varPortNode = nullptr;

            string direction;
            string dataType;
            string netType;
            string portType;

            getNode(*portDecNode, SyntaxKind::NetPortHeader, netPortNode);
            getNode(*portDecNode, SyntaxKind::VariablePortHeader, varPortNode);
            if (netPortNode != nullptr) {
                portType = "net";

                const auto &portHeader = netPortNode->as<NetPortHeaderSyntax>();

                direction = string(portHeader.direction.valueText());

                const DataTypeSyntax *dataTypeNode = portHeader.dataType;

                if (dataTypeNode->kind == SyntaxKind::NamedType) {
                    const auto &namedType = dataTypeNode->as<NamedTypeSyntax>();
                    dataType = string(namedType.name->getFirstToken().valueText());
                } else if (dataTypeNode->kind == SyntaxKind::ImplicitType) {
                    dataType = "wire";
                } else if (dataTypeNode->kind == SyntaxKind::RegType){
                    dataType = "reg";
                } else {
                    cout << "found data type node as kind: " << toString(dataTypeNode->kind) << endl;
                    cout << "new data type likely needs to be implemented." << endl;

                }

                netType = string(portHeader.netType.valueText());
            } else if (varPortNode != nullptr) {
                cout << "var" << endl;
                portType = "var";

                const auto &portHeader = varPortNode->as<VariablePortHeaderSyntax>();

                direction = string(portHeader.direction.valueText());

                const DataTypeSyntax *dataTypeNode = portHeader.dataType;

                if (dataTypeNode->kind == SyntaxKind::NamedType) {
                    const auto &namedType = dataTypeNode->as<NamedTypeSyntax>();
                    dataType = string(namedType.name->getFirstToken().valueText());
                } else if (dataTypeNode->kind == SyntaxKind::ImplicitType) {
                    dataType = "wire";
                } else if (dataTypeNode->kind == SyntaxKind::RegType){
                    dataType = "reg";
                } else {
                    cout << "found data type node as kind: " << toString(dataTypeNode->kind) << endl;
                    cout << "new data type likely needs to be implemented." << endl;

                }

                // no port header for variable ports :(
            } else {
                cout << "didn't find either a net port header nor var port header. something went wrong." << endl;
            }

            cout << "direction: " << direction << endl;
            cout << "data type: " << dataType << endl;
            cout << "net type: " << netType << endl;

            vector<const SyntaxNode *> dimNodes;
            getAllNodes(*portDecNode, SyntaxKind::IntegerLiteralExpression, dimNodes);

            int startDim = 0;
            int endDim = 0;

            if (dimNodes.size() == 2) {
                const auto &startDimNode = dimNodes[0]->as<LiteralExpressionSyntax>();
                const auto &endDimNode = dimNodes[1]->as<LiteralExpressionSyntax>();

                startDim = (int) startDimNode.literal.intValue().toFloat();
                endDim = (int) endDimNode.literal.intValue().toFloat();

            } else {
                cout << "didn't find 2 dimensions" << endl;
            }

            cout << "dimension: [" << startDim << ":" << endDim << "]" << endl;

            vector<const SyntaxNode *> identNodes;
            getAllNodes(*portDecNode, SyntaxKind::Declarator, identNodes);
//            traverseNode(*portDecNode, 0);

            vector<string> identifiers;
            for (const SyntaxNode *node: identNodes) {
                auto decl = &node->as<DeclaratorSyntax>();
                identifiers.push_back(string(decl->name.valueText()));
                cout << "identifier: " << string(decl->name.valueText()) << endl;
            }

            // Generate a unique handle
            static int portCounter = 0;
            std::string handleStr = "port" + std::to_string(portCounter++);

            // assemble port instance
            auto port = make_unique<Port>(portDecNode, portType, direction, netType, dataType, startDim, endDim, identifiers);

            // Store the instance in the map
            ports[handleStr] = std::move(port);

            portStrings.push_back(handleStr);

        }

        return portStrings;
    }

};

class Tree {
public:

    const SyntaxNode* root;

    explicit Tree(const SyntaxNode* root) {
        this->root = root;
    }

    optional<string> getModule(const string& moduleName) {
        vector<const SyntaxNode*> modNodes;

        getAllNodes(*this->root, SyntaxKind::ModuleDeclaration, modNodes);
        if (modNodes.empty()) {
            cout << "Failed to collect module nodes." << endl;
        }

        unique_ptr<Module> mod = nullptr;
        for (const SyntaxNode* node : modNodes) {
            const auto& moduleDec = node->as<ModuleDeclarationSyntax>();
            if (moduleDec.header->name.valueText() == moduleName) {
                mod = make_unique<Module>(moduleName, &moduleDec);
                break;
            }
        }

        // failed to find module with given name

        if (mod == nullptr) return nullopt;

        // Generate a unique handle for this instance

        // Generate a unique handle
        static int modCounter = 0;
        std::string handleStr = "mod" + std::to_string(modCounter++);

        // Store the instance in the map
        modules[handleStr] = std::move(mod);

        return handleStr.c_str();
    };

};



/*
Hi Ben,
What I would like to have in TCL:
set tree [slang_parse "file.v"]
set module [$tree get_module "memory"]
puts [$module name]
set ports [$module get_ports]   ; # will return all the ports
foreach p $ports { puts "port [$p name] [$p direction] [$p width]" }
*/

void getNode(const SyntaxNode& node, SyntaxKind kind, const SyntaxNode*& searchNode) {
    // Iterate over children using getChildCount() and childNode()
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        // Get the child node
        const SyntaxNode* child = node.childNode(i);

        // Check if the child is valid and matches the desired kind
        if (child && (child->kind == kind)) {
            searchNode = child; // Assign the found node to searchNode
            return;             // Stop further traversal
        }

        // Recursive traversal of child nodes
        if (child) {
            getNode(*child, kind, searchNode);
            if (searchNode) {
                return; // Stop if the desired node has been found
            }
        }
    }
}

void getAllNodes(const SyntaxNode& node, SyntaxKind kind, std::vector<const SyntaxNode*>& nodes) {
    // Iterate over children using getChildCount() and childNode()
    int childCount = node.getChildCount();
    for (int i = 0; i < childCount; ++i) {
        // Get the child node
        const SyntaxNode* child = node.childNode(i);

        if (!child) continue;  // Ensure the child is valid
        // Check if the child is valid and matches the desired kind
        if (child->kind == kind) {
            nodes.push_back(child); // Add the matching node to the vector
        }

        getAllNodes(*child, kind, nodes);
    }
}

// Recursive function to print and traverse SyntaxNode children
void traverseNode(const SyntaxNode& node, int depth) {
    // Indentation for hierarchy visualization
    for (int i = 0; i < depth; ++i) {
        cout << "  ";
    }

    // Print node kind
    cout << "Node Kind: " << toString(node.kind) << endl;
    for (int i = 0; i < depth; ++i) {
        cout << "  ";
    }
    cout << "Node: " << node.toString() << endl;
    cout << endl;

    // Iterate over children using getChildCount() and childNode()
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        const SyntaxNode* child = node.childNode(i);
        if (child) {
            traverseNode(*child, depth + 1); // Recursive traversal
        }
    }
}


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
    const SyntaxNode& root = syntaxTree->root();

    // Create a new Tree instance
    auto tree = std::make_unique<Tree>(&root);

//    traverseNode(root, 0);

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

    bool data_method = (method == "portType" || method == "direction" || method == "netType" || method == "dataType" || method == "startDim" || method == "endDim" || method == "identifiers" || method == "string");

    if (data_method) {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <handle> <data_member>"), TCL_STATIC);
            return TCL_ERROR;
        }

        if (method == "portType") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->portType.c_str(), port->portType.length()));
        } else if (method == "direction") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->direction.c_str(), port->direction.length()));
        } else if (method == "netType") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->netType.c_str(), port->netType.length()));
        }  else if (method == "dataType") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(port->dataType.c_str(), port->dataType.length()));
        } else if (method == "startDim") {
            Tcl_SetObjResult(interp, Tcl_NewIntObj(port->startDim));
        }  else if (method == "endDim") {
            Tcl_SetObjResult(interp, Tcl_NewIntObj(port->endDim));
        }  else if (method == "identifiers") {
            const int ident_len = port->identifiers.size();
            Tcl_Obj* identObjs[ident_len];
            for (int i=0; i < ident_len; i++) {
                string ident = port->identifiers[i];
                identObjs[i] = Tcl_NewStringObj(ident.c_str(), ident.length());
            }

            Tcl_SetObjResult(interp, Tcl_NewListObj(ident_len, identObjs));
        } else if (method == "string") {
            string portString = port->getString();
            Tcl_SetObjResult(interp, Tcl_NewStringObj(portString.c_str(), portString.length()));
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
