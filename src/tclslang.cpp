// TCLSLANG BY BEN STAHL
// https://github.com/benastahl/tclslang
//
// Tcl binding: every design object is exposed as a Tcl command (its handle).
//
//   set tree [slang_parse top.sv]
//   set mod  [$tree get_module top]
//   foreach p [$mod get_ports] { puts [$p name] }
//   $tree destroy                ;# frees the tree and every handle from it
//
// Each command's clientData is the object itself, and its delete proc frees
// the object, so `$h destroy`, `rename $h {}` and interpreter teardown all
// release memory the same way.

#include <array>
#include <string>
#include <vector>

#include <tcl.h>

#include "tclslang/design.hpp"

using namespace tclslang;
using Kind = Object::Kind;

namespace {

int ObjectCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]);
void ObjectDeleted(ClientData clientData);

Tcl_Obj* str(std::string_view text) {
    return Tcl_NewStringObj(text.data(), static_cast<int>(text.size()));
}

int setResult(Tcl_Interp* interp, std::string_view text) {
    Tcl_SetObjResult(interp, str(text));
    return TCL_OK;
}

int error(Tcl_Interp* interp, const std::string& message) {
    Tcl_SetObjResult(interp, str(message));
    return TCL_ERROR;
}

// Registers `object` as a Tcl command the first time it is handed to a script
// and returns its handle.
Tcl_Obj* expose(Tcl_Interp* interp, Object* object) {
    static constexpr std::array<const char*, 6> prefixes = {"tree", "mod",  "cell",
                                                            "port", "conn", "driver"};
    static std::array<int, 6> counters{};

    if (!object->command) {
        const auto kind = static_cast<size_t>(object->kind);
        object->handle = prefixes[kind] + std::to_string(counters[kind]++);
        object->command = Tcl_CreateObjCommand(interp, object->handle.c_str(), ObjectCmd, object,
                                               ObjectDeleted);
    }
    return str(object->handle);
}

template<typename T>
int setList(Tcl_Interp* interp, const std::vector<T*>& objects) {
    Tcl_Obj* list = Tcl_NewListObj(0, nullptr);
    for (T* object : objects) Tcl_ListObjAppendElement(interp, list, expose(interp, object));
    Tcl_SetObjResult(interp, list);
    return TCL_OK;
}

// Frees `object` and everything below it. Children with a live command are
// deleted through Tcl, which calls back into ObjectDeleted for them.
void destroyTree(Object* object) {
    const std::vector<Object*> children = object->children;
    for (Object* child : children) {
        if (child->command)
            Tcl_DeleteCommandFromToken(object->tree.interp, child->command);
        else
            destroyTree(child);
    }

    if (object->kind == Kind::Tree)
        delete static_cast<Tree*>(object);
    else
        object->tree.release(object);
}

void ObjectDeleted(ClientData clientData) {
    auto* object = static_cast<Object*>(clientData);
    object->command = nullptr;
    destroyTree(object);
}

// Looks up `objv[1]` in `methods`; on failure leaves Tcl's standard
// "bad method ... must be ..." message in the result. Each table lists
// "destroy" last so it shows up in that message; ObjectCmd handles it before
// dispatching, so the per-kind switches never see it.
bool getMethod(Tcl_Interp* interp, Tcl_Obj* const objv[], const char* const methods[], int& index) {
    return Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, &index) == TCL_OK;
}

bool checkArgs(Tcl_Interp* interp, int objc, Tcl_Obj* const objv[], int expected,
               const char* usage = nullptr) {
    if (objc == expected) return true;
    Tcl_WrongNumArgs(interp, 2, objv, usage);
    return false;
}

// --- per-kind methods -------------------------------------------------------

int TreeMethod(Tree& tree, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    static const char* const methods[] = {"get_module", "top_modules", "diagnostics", "destroy", nullptr};
    enum { GET_MODULE, TOP_MODULES, DIAGNOSTICS };
    int method;
    if (!getMethod(interp, objv, methods, method)) return TCL_ERROR;

    switch (method) {
        case GET_MODULE: {
            if (!checkArgs(interp, objc, objv, 3, "module_name")) return TCL_ERROR;
            const std::string name = Tcl_GetString(objv[2]);
            Instance* module = tree.findModule(name);
            if (!module) return error(interp, "module \"" + name + "\" not found");
            Tcl_SetObjResult(interp, expose(interp, module));
            return TCL_OK;
        }
        case TOP_MODULES: {
            if (!checkArgs(interp, objc, objv, 2)) return TCL_ERROR;
            Tcl_Obj* list = Tcl_NewListObj(0, nullptr);
            for (const auto& name : tree.topModules())
                Tcl_ListObjAppendElement(interp, list, str(name));
            Tcl_SetObjResult(interp, list);
            return TCL_OK;
        }
        case DIAGNOSTICS:
            if (!checkArgs(interp, objc, objv, 2)) return TCL_ERROR;
            return setResult(interp, tree.diagnostics());
    }
    return TCL_ERROR;
}

int InstanceMethod(Instance& instance, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    static const char* const methods[] = {"name",      "ref_name",  "hier_path",
                                          "get_ports", "get_cells", "get_connections", "destroy", nullptr};
    enum { NAME, REF_NAME, HIER_PATH, GET_PORTS, GET_CELLS, GET_CONNECTIONS };
    int method;
    if (!getMethod(interp, objv, methods, method)) return TCL_ERROR;
    if (!checkArgs(interp, objc, objv, 2)) return TCL_ERROR;

    switch (method) {
        case NAME: return setResult(interp, instance.name);
        case REF_NAME: return setResult(interp, instance.refName());
        case HIER_PATH: return setResult(interp, instance.path);
        case GET_PORTS: return setList(interp, instance.ports());
        case GET_CELLS: return setList(interp, instance.cells());
        case GET_CONNECTIONS: return setList(interp, instance.connections());
    }
    return TCL_ERROR;
}

int PortMethod(Port& port, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    static const char* const methods[] = {"name",     "direction", "kind",      "data_type",
                                          "net_type", "width",     "dimensions", "interface",
                                          "modport",  "destroy", nullptr};
    enum { NAME, DIRECTION, KIND, DATA_TYPE, NET_TYPE, WIDTH, DIMENSIONS, INTERFACE, MODPORT };
    int method;
    if (!getMethod(interp, objv, methods, method)) return TCL_ERROR;
    if (!checkArgs(interp, objc, objv, 2)) return TCL_ERROR;

    switch (method) {
        case NAME: return setResult(interp, port.name);
        case DIRECTION: return setResult(interp, port.direction);
        case KIND: return setResult(interp, port.kind);
        case DATA_TYPE: return setResult(interp, port.dataType);
        case NET_TYPE: return setResult(interp, port.netType);
        case WIDTH: Tcl_SetObjResult(interp, Tcl_NewWideIntObj(Tcl_WideInt(port.width))); return TCL_OK;
        case INTERFACE: return setResult(interp, port.interfaceName);
        case MODPORT: return setResult(interp, port.modport);
        case DIMENSIONS: {
            Tcl_Obj* dims = Tcl_NewListObj(0, nullptr);
            for (const auto& [left, right] : port.dimensions) {
                Tcl_Obj* range[2] = {Tcl_NewIntObj(left), Tcl_NewIntObj(right)};
                Tcl_ListObjAppendElement(interp, dims, Tcl_NewListObj(2, range));
            }
            Tcl_SetObjResult(interp, dims);
            return TCL_OK;
        }
    }
    return TCL_ERROR;
}

int ConnectionMethod(Connection& conn, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    static const char* const methods[] = {"name", "get_port", "get_driver", "destroy", nullptr};
    enum { NAME, GET_PORT, GET_DRIVER };
    int method;
    if (!getMethod(interp, objv, methods, method)) return TCL_ERROR;
    if (!checkArgs(interp, objc, objv, 2)) return TCL_ERROR;

    switch (method) {
        case NAME: return setResult(interp, conn.name());
        case GET_PORT: Tcl_SetObjResult(interp, expose(interp, conn.port())); return TCL_OK;
        case GET_DRIVER: {
            Driver* driver = conn.driver();
            if (!driver) return setResult(interp, "");  // unconnected
            Tcl_SetObjResult(interp, expose(interp, driver));
            return TCL_OK;
        }
    }
    return TCL_ERROR;
}

int DriverMethod(Driver& driver, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    static const char* const methods[] = {"name",     "kind",      "expr",    "const",
                                          "data_type", "net_type", "modport", "destroy", nullptr};
    enum { NAME, KIND, EXPR, CONSTANT, DATA_TYPE, NET_TYPE, MODPORT };
    int method;
    if (!getMethod(interp, objv, methods, method)) return TCL_ERROR;
    if (!checkArgs(interp, objc, objv, 2)) return TCL_ERROR;

    // Methods that only make sense for some driver kinds.
    const auto only = [&](std::initializer_list<std::string_view> kinds) {
        for (auto k : kinds)
            if (driver.kind == k) return true;
        error(interp, std::string(Tcl_GetString(objv[1])) + " is not available on a " +
                          driver.kind + " driver");
        return false;
    };

    switch (method) {
        case NAME: return setResult(interp, driver.name());
        case KIND: return setResult(interp, driver.kind);
        case EXPR: return setResult(interp, driver.text());
        case CONSTANT:
            if (!only({"const"})) return TCL_ERROR;
            return setResult(interp, driver.constant);
        case DATA_TYPE:
            if (!only({"const", "var", "net", "expr"})) return TCL_ERROR;
            return setResult(interp, driver.dataType());
        case NET_TYPE:
            if (!only({"net"})) return TCL_ERROR;
            return setResult(interp, driver.netType());
        case MODPORT:
            if (!only({"interface"})) return TCL_ERROR;
            return setResult(interp, driver.modport);
    }
    return TCL_ERROR;
}

// Every handle command: `destroy` is common, the rest is per kind.
int ObjectCmd(ClientData clientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    auto* object = static_cast<Object*>(clientData);
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "method ?arg ...?");
        return TCL_ERROR;
    }

    if (std::string_view(Tcl_GetString(objv[1])) == "destroy") {
        if (!checkArgs(interp, objc, objv, 2)) return TCL_ERROR;
        Tcl_DeleteCommandFromToken(interp, object->command);  // frees `object`
        Tcl_ResetResult(interp);
        return TCL_OK;
    }

    switch (object->kind) {
        case Kind::Tree: return TreeMethod(static_cast<Tree&>(*object), interp, objc, objv);
        case Kind::Module:
        case Kind::Cell:
            return InstanceMethod(static_cast<Instance&>(*object), interp, objc, objv);
        case Kind::Port: return PortMethod(static_cast<Port&>(*object), interp, objc, objv);
        case Kind::Connection:
            return ConnectionMethod(static_cast<Connection&>(*object), interp, objc, objv);
        case Kind::Driver: return DriverMethod(static_cast<Driver&>(*object), interp, objc, objv);
    }
    return TCL_ERROR;
}

int SlangParseCmd(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[]) {
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "file ?file ...?");
        return TCL_ERROR;
    }

    std::vector<std::string> paths;
    for (int i = 1; i < objc; i++) paths.emplace_back(Tcl_GetString(objv[i]));

    std::string message;
    std::unique_ptr<Tree> tree = Tree::parse(paths, message);
    if (!tree) return error(interp, "slang_parse: " + message);

    tree->interp = interp;
    // Ownership passes to the command; ObjectDeleted frees it.
    Tcl_SetObjResult(interp, expose(interp, tree.release()));
    return TCL_OK;
}

} // namespace

// Initialization function required by Tcl
extern "C" [[maybe_unused]] int Tclslang_Init(Tcl_Interp* interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "slang_parse", SlangParseCmd, nullptr, nullptr);
    return Tcl_PkgProvide(interp, "tclslang", "2.0");
}
