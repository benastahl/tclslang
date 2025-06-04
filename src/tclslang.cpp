//
// Created by ec2-user on 12/29/24.
//

// TCLSLANG BY BEN STAHL
// https://github.com/benastahl/tclslang

#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxNode.h"
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/types/NetType.h>
#include <cstring>
#include <string>
#include <utility>
#include <tcl.h>
#include <tclslang/hdl_tree.hpp>

using namespace slang::syntax;
using namespace slang::ast;
using namespace std;

int Tree_MethodCmd(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[]);
int Module_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
int Port_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
int Cell_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
int PortConn_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);
int Driver_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]);


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

 NEW:
mod get_cells -> {instance1_inst instance2_inst}
inst get_connections -> {conn_inst1 conn_inst2}
 conn_inst get_driver -> driver_inst
 */


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
            cerr << "Module name not found: \"" << moduleName << '"' << endl;
            Tcl_SetResult(interp, nullptr, TCL_STATIC); // empty string in tcl
            return TCL_OK;  // have tcl script-writer handle the error
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
    } else if (method == "print_tree") {

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
    } else if (method == "get_connections") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <cell_name> get_connections"), TCL_STATIC);
            return TCL_ERROR;
        }

        // collect cells
        vector<string> connHandles = cell->getPortConns();

        // convert cells to Tcl_StringObj
        const unsigned int conns_size = connHandles.size();
        Tcl_Obj* cellsObjForm[conns_size];
        for (int i=0; i < conns_size; i++) {
            const string connHandle = connHandles[i];
            cellsObjForm[i] = Tcl_NewStringObj(connHandle.c_str(), connHandle.length());

            Tcl_CreateCommand(interp, connHandle.c_str(), PortConn_MethodCmd, (ClientData)&connections[connHandle], nullptr);
        }

        Tcl_SetObjResult(interp, Tcl_NewListObj(conns_size, cellsObjForm));
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
            const string& cellHandle = cellHandles[i];
            cellsObjForm[i] = Tcl_NewStringObj(cellHandle.c_str(), cellHandle.length());

            Tcl_CreateCommand(interp, cellHandle.c_str(), Cell_MethodCmd, (ClientData)&cells[cellHandle], nullptr);
        }

        Tcl_SetObjResult(interp, Tcl_NewListObj(cells_size, cellsObjForm));
        return TCL_OK;
    }

    Tcl_SetResult(interp, const_cast<char*>("Unknown method"), TCL_STATIC);
    return TCL_ERROR;
}

int PortConn_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]) {
    if (argc < 2) {
        Tcl_SetResult(interp, const_cast<char *>("Usage: <connection_name> <method> [args...]"), TCL_STATIC);
        return TCL_ERROR;
    }

    // Get the handle name (e.g., "module0")
    std::string handle = argv[0];

    // Find the corresponding Module instance
    auto it = connections.find(handle);
    if (it == connections.end()) {
        Tcl_SetResult(interp, const_cast<char *>("Invalid connection handle"), TCL_STATIC);
        return TCL_ERROR;
    }

    auto &connection = it->second;

    // handle method called
    std::string method = argv[1];
    if (method == "name") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <connection_handle> name"), TCL_STATIC);
            return TCL_ERROR;
        }

        string portConnName = string(connection->portConn->port.name); // idk why im using port name lol

        Tcl_SetObjResult(interp, Tcl_NewStringObj(portConnName.c_str(), portConnName.length()));
        return TCL_OK;
    } else if (method == "get_port"){
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <connection_handle> get_port"), TCL_STATIC);
            return TCL_ERROR;
        }

        string portHandle = connection->portHandle;

        Tcl_CreateCommand(interp, portHandle.c_str(), Port_MethodCmd, (ClientData)&ports[portHandle], nullptr);

        Tcl_SetObjResult(interp, Tcl_NewStringObj(portHandle.c_str(), portHandle.length()));
        return TCL_OK;
    } else if (method == "get_driver") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <connection_handle> get_driver"), TCL_STATIC);
            return TCL_ERROR;
        }

        string driverHandle = connection->driverHandle;

        Tcl_CreateCommand(interp, driverHandle.c_str(), Driver_MethodCmd, (ClientData)&drivers[driverHandle], nullptr);

        Tcl_SetObjResult(interp, Tcl_NewStringObj(driverHandle.c_str(), driverHandle.length()));
        return TCL_OK;
    }

    Tcl_SetResult(interp, const_cast<char*>("Unknown method"), TCL_STATIC);
    return TCL_ERROR;
}

int Driver_MethodCmd(ClientData clientData, Tcl_Interp* interp, int argc, const char* argv[]) {
    if (argc < 2) {
        Tcl_SetResult(interp, const_cast<char *>("Usage: <driver_handle> <method> [args...]"), TCL_STATIC);
        return TCL_ERROR;
    }

    // Get the handle name (e.g., "module0")
    std::string handle = argv[0];

    // Find the corresponding Module instance
    auto it = drivers.find(handle);
    if (it == drivers.end()) {
        Tcl_SetResult(interp, const_cast<char *>("Invalid driver handle"), TCL_STATIC);
        return TCL_ERROR;
    }

    auto &driver = it->second;

    // handle method called
    std::string method = argv[1];
    string driver_type = driver->type;

    if (method == "name") {
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <driver_handle> name"), TCL_STATIC);
            return TCL_ERROR;
        }

        if (driver_type == "const") {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("", 0));
            return TCL_OK;
        }

        string driver_name = string(driver->driverSymbol->name);
        Tcl_SetObjResult(interp, Tcl_NewStringObj(driver_name.c_str(), driver_name.size()));
        return TCL_OK;
    } else if (method == "type") {  // either "var" or "net"
        if (argc != 2) {
            Tcl_SetResult(interp, const_cast<char*>("Usage: <driver_handle> type"), TCL_STATIC);
            return TCL_ERROR;
        }

        Tcl_SetObjResult(interp, Tcl_NewStringObj(driver_type.c_str(), driver_type.size()));
        return TCL_OK;
    }

    // DRIVER TYPE HANDLING (either var or net)

    /*
    logic a;       // Variable
     type -> logic

    wire logic b;  // Net
     type -> logic
     netType -> wire
     */


    // const specific commands
    if (driver_type == "const") {

        if (method == "const") {
            string constValue = string(driver->constant);

            Tcl_SetObjResult(interp, Tcl_NewStringObj(constValue.c_str(), constValue.size()));
            return TCL_OK;
        }
    }

    // var specific methods
    if (driver_type == "var" && driver->driverSymbol->kind == SymbolKind::Variable) {
        const VariableSymbol& varSymbol = driver->driverSymbol->as<VariableSymbol>();
        if (method == "data_type") {
            string dataType = string(varSymbol.getType().toString());

            Tcl_SetObjResult(interp, Tcl_NewStringObj(dataType.c_str(), dataType.size()));
            return TCL_OK;
        }
    }

    // net specific methods
    if (driver_type == "net" && driver->driverSymbol->kind == SymbolKind::Net) {
        const NetSymbol& netSymbol = driver->driverSymbol->as<NetSymbol>();

        if (method == "data_type") {
            string dataType = string(netSymbol.getType().toString());

            Tcl_SetObjResult(interp, Tcl_NewStringObj(dataType.c_str(), dataType.size()));
            return TCL_OK;
        } else if (method == "net_type") {
            string netType = string(netSymbol.netType.name);

            Tcl_SetObjResult(interp, Tcl_NewStringObj(netType.c_str(), netType.size()));
            return TCL_OK;
        }
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
