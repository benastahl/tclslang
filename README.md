# tclslang

[![CI](https://github.com/benastahl/tclslang/actions/workflows/ci.yml/badge.svg)](https://github.com/benastahl/tclslang/actions/workflows/ci.yml)

**Query elaborated SystemVerilog designs from Tcl.** tclslang is a Tcl extension built on the [slang](https://github.com/MikePopoloski/slang) compiler. It makes a design's modules, ports, instances and connectivity scriptable in the language that EDA flows already use. The design is fully elaborated, so parameters, generate blocks, instance arrays and interfaces are resolved the same way a simulator resolves them.

Developed by **Ben Stahl** · [github.com/benastahl/tclslang](https://github.com/benastahl/tclslang)

```tcl
load build/libtclslang.so

set tree [slang_parse rtl/top.sv rtl/alu.sv]
set top  [$tree get_module top]

foreach cell [$top get_cells] {
    puts "[$cell name] : [$cell ref_name]"
    foreach conn [$cell get_connections] {
        set d [$conn get_driver]
        puts "  .[$conn name] <- [expr {$d eq "" ? "(unconnected)" : [$d expr]}]"
    }
}
$tree destroy
```

Running [`examples/dump_hierarchy.tcl`](examples/dump_hierarchy.tcl) on [`tests/cases/generate_and_arrays.sv`](tests/cases/generate_and_arrays.sv):

```text
module top
  port d              input   logic[3:0]
  u_array[0] : cell_a    (top.u_array[0])
    .d            input   <- d[1:0]  (var logic[3:0])
  u_array[1] : cell_a    (top.u_array[1])
    .d            input   <- d[1:0]  (var logic[3:0])
  g_loop[0].u_gen : cell_a    (top.g_loop[0].u_gen)
    .d            input   <- d[i]  (var logic[3:0])
  g_loop[1].u_gen : cell_a    (top.g_loop[1].u_gen)
    .d            input   <- d[i]  (var logic[3:0])
  g_if.u_cond : cell_a    (top.g_if.u_cond)
    .d            input   <- d[3]  (var logic[3:0])
```

---

## Contents

- [Quick start](#quick-start)
- [API reference](#api-reference)
- [Errors and diagnostics](#errors-and-diagnostics)
- [Handle lifetime](#handle-lifetime)
- [Testing and CI](#testing-and-ci)
- [How it works](#how-it-works)
- [Phase 1 changes (before and after)](#phase-1-changes-before-and-after)
- [Roadmap](#roadmap)

---

## Quick start

### Docker (recommended)

The image is Rocky Linux 9, which is binary-compatible with RHEL 9. It builds fmt, slang (pinned) and tclslang, then runs the test suite:

```bash
docker build -t tclslang .      # ~3 min the first time; slang is the slow part
docker run --rm tclslang        # runs the full test suite (ctest)
```

To work on your own checkout inside the container, mount it and rebuild there:

```bash
docker run --rm -it -v "$PWD":/work tclslang bash
# inside the container:
make test                                          # configure, build, run ctest
tclsh examples/dump_hierarchy.tcl my_design.sv
```

The same targets are available from the host through the Makefile: `make docker` and `make docker-test`.

### Native build on RHEL / Rocky / Alma 9

The [Dockerfile](Dockerfile) is the reference. These are the same steps:

```bash
sudo dnf install -y dnf-plugins-core && sudo dnf config-manager --set-enabled crb
sudo dnf install -y gcc-c++ make cmake git python3 tcl tcl-devel pkgconf-pkg-config

# fmt >= 11.1 as a shared library (slang and tclslang must link the same one)
git clone --depth 1 --branch 11.1.4 https://github.com/fmtlib/fmt.git
cmake -S fmt -B fmt/build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DFMT_TEST=OFF
cmake --build fmt/build -j && sudo cmake --install fmt/build

# slang, at the commit tclslang is tested against
git clone https://github.com/MikePopoloski/slang.git && git -C slang checkout 652a9ab5b4d093ff4f8fcf6e1e3fdcc7e292f931
cmake -S slang -B slang/build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DSLANG_INCLUDE_TESTS=OFF -DSLANG_INCLUDE_TOOLS=OFF -DSLANG_USE_MIMALLOC=OFF
cmake --build slang/build -j && sudo cmake --install slang/build --strip

# /usr/local/lib64 is not on the default loader path on EL
echo /usr/local/lib64 | sudo tee /etc/ld.so.conf.d/local.conf && sudo ldconfig

make test    # in this repo
```

**Requirements:** a C++20 compiler (GCC 11 or newer), CMake 3.20 or newer, Tcl 8.6, fmt 11.1 or newer, and slang.

---

## API reference

Every design object is a Tcl command, called a *handle*, and you call methods on it: `$handle method ?args?`. Each object has exactly one handle: asking for the same port twice returns the same handle. An unknown method returns Tcl's standard error listing the valid methods.

### `slang_parse file ?file ...?` → tree

Parses and elaborates the files as one design. It raises a Tcl error if a file can't be read or if the design has errors (see [Errors and diagnostics](#errors-and-diagnostics)).

### Tree

| Method | Returns |
|---|---|
| `get_module name` | Module handle. Returns a top-level instance of `name` if there is one, otherwise the shallowest instance of it. Raises an error if the design has none. |
| `top_modules` | List of the design's top-level module names |
| `diagnostics` | slang's formatted warnings, or `""` if there are none |
| `destroy` | Frees the tree **and every handle created from it** |

### Module and cell

`get_module` returns a module. `get_cells` returns cells, which are the instances directly inside another instance, including instances in generate blocks and instance arrays.

| Method | Returns |
|---|---|
| `name` | Module: the module name. Cell: its path below the parent, e.g. `u_core`, `g_loop[0].u_gen`, `u_array[1]` |
| `ref_name` | The module or interface being instantiated, e.g. `cell_a` |
| `hier_path` | Full hierarchical path, e.g. `top.g_loop[0].u_gen` |
| `get_ports` | Port handles, in declaration order |
| `get_cells` | Cell handles for the instances directly inside this one |
| `get_connections` | Connection handles, one per port of this instance |
| `destroy` | Frees this handle and everything obtained through it |

### Port

| Method | Returns | Example |
|---|---|---|
| `name` | Port name | `data_in` |
| `direction` | `input`, `output`, `inout` or `ref` (`""` for interface ports) | `input` |
| `kind` | `net`, `var`, `interface` or `multiport` | `net` |
| `data_type` | slang's type | `logic[7:0]` |
| `net_type` | Net type for net ports, otherwise `""` | `wire` |
| `width` | Total bits, including unpacked dimensions | `8` |
| `dimensions` | `{left right}` for each dimension, outermost first | `{{0 15} {3 0}}` |
| `interface` | Interface name for interface ports | `bus_if` |
| `modport` | Modport for interface ports, if any | `master` |

The difference between `net` and `var` is the one SystemVerilog makes: `input wire a` and `input a` are nets, while `input logic a` and `output reg a` are variables.

### Connection

| Method | Returns |
|---|---|
| `name` | Name of the port being connected |
| `get_port` | The port handle, the same one `$cell get_ports` returns |
| `get_driver` | Driver handle, or `""` if the port is unconnected |

### Driver

A driver describes what a port is hooked up to. `kind` is one of:

| `kind` | Meaning | Example connection |
|---|---|---|
| `var` | a variable, or a select of one | `.a(state)`, `.a(bus[3:2])` |
| `net` | a net, or a select of one | `.a(wire_sig)` |
| `const` | a constant expression | `.a(1'b1)`, `.a(WIDTH)` |
| `expr` | any other expression | `.a({x, z})`, `.a(x & z)` |
| `interface` | an interface instance, optionally through a modport | `.bus(the_bus.slave)` |

| Method | Available on | Returns |
|---|---|---|
| `kind` | all | See the table above |
| `expr` | all | Source text of the connection, e.g. `bus[3:2]` |
| `name` | all | Signal or interface name (`""` for `const` and `expr`) |
| `data_type` | `var`, `net`, `const`, `expr` | Declared type of the signal, or the type of the expression |
| `net_type` | `net` | e.g. `wire` |
| `const` | `const` | The value, e.g. `1'b1` |
| `modport` | `interface` | The modport name, or `""` |

Calling a method on a driver kind it doesn't apply to raises an error, for example `net_type is not available on a var driver`.

---

## Errors and diagnostics

Errors are reported through slang's own formatted diagnostics:

```text
% slang_parse tests/cases/err_trailing_comma.v
slang_parse: design has 1 error(s)
tests/cases/err_trailing_comma.v:9:48: error: misplaced trailing ','
    input wire [7:9] i_select, hello_there_haha,
                                               ^
% slang_parse /no/such/file.sv
slang_parse: cannot read "/no/such/file.sv": No such file or directory
% $tree get_module nope
module "nope" not found
```

Warnings don't fail the parse. They're available through `$tree diagnostics`.

---

## Handle lifetime

Every handle points into the slang compilation owned by its tree, so the tree owns every handle made from it:

```tcl
set tree [slang_parse top.sv]
set cells [[$tree get_module top] get_cells]
$tree destroy          ;# frees the compilation and deletes every handle above
info commands cell*    ;# -> nothing left
```

- `$h destroy`, `rename $h {}` and interpreter teardown all free memory the same way.
- Destroying a handle also destroys its descendants. For example, destroying a module destroys the cells and ports you got through it.
- Calling a destroyed handle is an ordinary `invalid command name` error, not a crash.

---

## Testing and CI

```bash
make test                                     # or: cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
TCLSLANG_UPDATE_GOLDEN=1 ctest --test-dir build   # regenerate golden files after an intentional change
```

| Suite | What it covers |
|---|---|
| **Golden** ([`tests/cases`](tests/cases)), 14 cases | Each `.sv`/`.v` file, or directory for multi-file designs, is dumped by [`tests/lib/dump.tcl`](tests/lib/dump.tcl) and diffed against `<case>.golden`. The dump prints names and attributes only, never handle IDs, so the goldens don't change when handle numbering does. The cases cover generate blocks, instance arrays, interface ports, expression connections, port shapes, repeated instances, a multi-file design, and four files with syntax or elaboration errors. |
| **API** ([`tests/api.test`](tests/api.test)), 32 tests | tcltest checks of every method, the error messages, handle identity, and lifetime: cascade destroy, `rename`, and parse/destroy in a loop without accumulating handles. |
| **Example** | [`examples/dump_hierarchy.tcl`](examples/dump_hierarchy.tcl) runs as a smoke test so it can't drift from the API. |

**Sanitizers.** `-DTCLSLANG_SANITIZE=ON` builds the extension with AddressSanitizer and UndefinedBehaviorSanitizer and preloads the ASan runtime into the test processes, because `tclsh` isn't instrumented. Leak detection is on. The only suppression, in [`tests/lsan.supp`](tests/lsan.supp), is glibc's internal `dlerror` buffer. Use `RelWithDebInfo`, not `Debug`. slang's headers change class layouts when `NDEBUG` is unset, so a Debug build linked against a Release slang crashes inside slang.

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTCLSLANG_SANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure
```

**CI** ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) builds the Docker image and runs the full suite twice on every push and PR: once as a Release build and once with ASan and UBSan. The GitHub Actions cache keeps the fmt and slang layers, so only tclslang rebuilds.

---

## How it works

```text
include/tclslang/design.hpp   slang-facing model: Tree, Instance, Port, Connection, Driver
src/design.cpp                elaboration, traversal, driver classification, diagnostics
src/tclslang.cpp              Tcl binding: one command per object, method dispatch, lifetime
tests/                        golden cases, dump library, tcltest API suite, LSan suppressions
examples/                     scripts that use the API
```

- **One owner.** A `Tree` owns its `SourceManager`, its syntax tree, its slang `Compilation`, and every object created from it. The `SourceManager` is declared first so it's destroyed last. Objects form a parent/child tree, so destroying a node destroys its subtree.
- **One object per thing.** Objects are cached by (kind, parent, slang pointer). That makes handles stable, and a connection's port is the same handle as the cell's port. Keying by parent also keeps cell paths correct when slang shares one body between identical instances.
- **Tcl-native commands.** `Tcl_CreateObjCommand` stores the object in `clientData`, and its delete proc frees that object's subtree. Method names are resolved with `Tcl_GetIndexFromObj`.
- **Traversal.** Cells are found by walking an instance body through generate blocks (skipping uninstantiated branches) and instance arrays, without descending into other instances.
- **Drivers.** Output and inout connections are bound by slang as assignments, and the driver is taken from the left-hand side. A connection is `const` if slang folded it to a constant, `var` or `net` if it references a single symbol, and `expr` otherwise. The text comes from the expression's source range, because slang synthesizes some nodes without syntax, such as implicit conversions and per-element array connections.

---

## Phase 1 changes (before and after)

Phase 1 took tclslang from a working prototype to something testable, reproducible and safe to script against. The commits on the `phase1` branch, in order:

| # | Commit | Summary |
|---|---|---|
| 0 | Docker + lifetime fix | RHEL-compatible Dockerfile; fixed a use-after-free that affected every handle |
| 1 | Repo hygiene | Orphan submodule, `.gitignore`, Makefile, example rename, net-driver print bug |
| 2 | Remove debug prints | The library no longer writes to stdout |
| 3 | Regression suite | Golden and tcltest suites in CTest; cases that were known to fail tracked as disabled |
| 4 | CI | GitHub Actions: Release plus ASan/UBSan, running in the Docker image |
| 4b | VLA fix | Undefined behavior found by the new UBSan job on its first run |
| 5 | Hardening + diagnostics | Crash fixes, generate/array/interface support, real error messages |
| 6 | Object model | Tree-owned handles, `destroy`, handle caching, `Tcl_CreateObjCommand` |
| 7 | API fixes | Correct port types, `ref_name`/`hier_path`, `get_module` for any module |
| 8 | README | This document |

### Every handle pointed at freed memory

`Tree::getModule` created slang's `Compilation` as a local variable. It was destroyed when `get_module` returned, and every module, port, cell and driver handle kept pointing into it. It seemed to work on the original machine because the freed memory hadn't been reused yet. On a fresh RHEL 9 container it crashed:

**Before**, running the original example script:
```text
   driver name: in_net
   driver type: in_net
   driver data_type: logic
terminate called after throwing an instance of 'std::length_error'
  what():  basic_string::_M_create
```

The same code built with AddressSanitizer fails on the first `get_ports` call:
```text
==116==ERROR: AddressSanitizer: heap-use-after-free on address 0xffffa5617368
READ of size 8 at 0xffffa5617368 thread T0
    #0 in Instance::getPorts[abi:cxx11]() const /src/include/tclslang/hdl_tree.hpp:282
    #1 in Module_MethodCmd(void*, Tcl_Interp*, int, char const**) /src/src/tclslang.cpp:170
```

**After:** the `Tree` owns the `Compilation`, and in step 6 everything else too. CI runs the suite under ASan, so this class of bug fails the build.

### Crashes on ordinary connections

**Before:** any connection that wasn't a plain signal, such as `.a({x, z})` or `.a(x & z)`, dereferenced a null symbol:
```text
bash: line 1:   115 Segmentation fault      tclsh before.tcl
```
Interface ports were cast to `PortSymbol`, which is undefined behavior.

**After:** these become `expr` and `interface` drivers:
```text
u_concat : leaf    (top.u_concat)
  .a            input   <- {x, z}  (expression, logic[1:0])
u_cons : consumer    (top.u_cons)
  .bus          interface <- the_bus  (interface, modport slave)
```

### Broken designs were accepted silently

**Before:**
```tcl
% slang_parse err_trailing_comma.v        ;# has a syntax error
tree0
% slang_parse /no/such/file.sv
Failed to parse Verilog file.
% $tree get_module nope                    ;# returns "" and prints to stderr
Failed to find module with given name.
Module name not found: "nope"
```

**After:** syntax and elaboration errors fail the parse with slang's diagnostics, and a missing module is a Tcl error:
```tcl
% slang_parse err_trailing_comma.v
slang_parse: design has 1 error(s)
err_trailing_comma.v:9:48: error: misplaced trailing ','
    input wire [7:9] i_select, hello_there_haha,
                                               ^
% slang_parse /no/such/file.sv
slang_parse: cannot read "/no/such/file.sv": No such file or directory
% $tree get_module nope
module "nope" not found
```

Four of the original seven test files turned out to contain syntax errors that had been parsing "successfully". They're now the error-path golden cases.

### Instances in generate blocks and arrays were invisible

**Before:** `get_cells` only looked at direct members of the module body, so a module built from `for` generate loops and instance arrays had no cells:
```tcl
% [[slang_parse generate_and_arrays.sv] get_module top] get_cells
                                           ;# empty
```

**After:** the traversal looks through generate blocks, skipping branches whose condition is false, and through instance arrays. Cells are named by their path:
```tcl
% lmap c [$top get_cells] {$c name}
{u_array[0]} {u_array[1]} {g_loop[0].u_gen} {g_loop[1].u_gen} g_if.u_cond
```

### Port types were guessed from the direction

**Before:** `type` returned `wire` for every input and `reg` for every output, whatever the declaration said. `dimType` was never set.
```text
in_var:  portType=Variable type=wire direction=In  dimType=<>     ;# declared `input logic`
out_var: portType=Variable type=reg  direction=Out dimType=<>     ;# declared `output logic`
```

**After:** the types come from slang:
```text
port in_var input var : logic width=1
port in_net input net : logic (wire) width=1
port addr input net : logic[3:0]$[0:15] (wire) width=64 dims={0 15} {3 0}
```

### Memory was never freed, and handles multiplied

**Before:** every object lived forever in a global map. Every call allocated new objects and registered new commands, even for the same port:
```tcl
% $m get_ports
port3 port4 port5
% $m get_ports
port6 port7 port8
```

**After:** handles are cached and owned by their tree:
```tcl
% expr {[$m get_ports] eq [$m get_ports]}
1
% $tree destroy; info commands port*
                                           ;# nothing left
```

The sanitizer job runs with leak detection on. As a check that it works, deliberately removing the `delete` of the tree makes the API suite fail with `8049662 byte(s) leaked in 6651 allocation(s)`.

### Library noise on stdout

**Before:** every query printed debugging output mixed into the script's own output, 522 lines for the seven original test files:
```text
InstanceSymbol:
	Name: top
	Kind: Instance
	Port List: address data_in data_out read_write chip_en
	Port Connections:
got instance: top of type Instance
Port Name: address
  Packed array range: 7 to 0
[Note] Getting driver...
```

**After:** the library prints nothing. Output belongs to the script.

### Tcl binding

| | Before | After |
|---|---|---|
| Command API | `Tcl_CreateCommand` (string args); every call looked its object up by name in a global map | `Tcl_CreateObjCommand`; the object is the command's `clientData` |
| Unknown method | `Unknown method` | `bad method "frobnicate": must be name, ref_name, hier_path, ...` |
| Result building | Variable-length arrays `Tcl_Obj* x[n]`, which UBSan flagged for `n == 0` | `Tcl_ListObjAppendElement` |
| Code layout | One header defining globals and non-`inline` functions, which broke as soon as a second `.cpp` included it | `design.hpp`/`design.cpp` (slang model) + `tclslang.cpp` (binding) |
| Warnings | Not enabled | `-Wall -Wextra`, clean |

### API migration (1.0 → 2.0)

| 1.0 | 2.0 |
|---|---|
| `$port portType` → `Net` / `Variable` | `$port kind` → `net` / `var` / `interface` / `multiport` |
| `$port type` → `wire`/`reg` (guessed) | `$port data_type` → `logic[7:0]`, plus `$port net_type` → `wire` |
| `$port dimType` (always empty) | removed |
| `$port direction` → `In` / `Out` / `InOut` | `input` / `output` / `inout` / `ref` |
| `$driver type` → `var` / `net` / `const` | `$driver kind`, which adds `expr` and `interface` |
| `$tree get_module x` → `""` if missing | Tcl error; also finds modules below the top |
| — | New: `destroy` on every handle, `$tree top_modules`, `$tree diagnostics`, `$cell ref_name`, `$cell hier_path`, `$port width`/`interface`/`modport`, `$driver expr`/`modport` |

### Build, tooling and repo

| | Before | After |
|---|---|---|
| Setup | 10 manual steps: yum, pip, conan, a slang build with `sudo cmake --install` | `docker build -t tclslang .`, or the native steps above with no conan |
| slang version | Whatever `git clone` fetched that day | Pinned to `652a9ab` in the Dockerfile and CI |
| `slang` directory | An orphan submodule pointer (no `.gitmodules`), always empty | Removed; slang is built where it's needed |
| Makefile | `-j$(nproc)` was an empty make variable, so `-j` was unlimited. `run` pointed at a nonexistent binary, `build` always ran `make clean` first, and the default script path was `./test.tcl  # default path lol` | `build`, `test`, `example`, `docker`, `docker-test` |
| `.gitignore` | `./build/` (never matched) | `build/` |
| Example | `yerrr.tcl`, which printed `[$d name]` as the driver type in the net branch | [`examples/dump_hierarchy.tcl`](examples/dump_hierarchy.tcl) |
| Tests | 7 `.v` files and a terminal paste (`expected_output.txt`) compared by eye | 14 golden cases + 32 API tests + an example smoke test in CTest |
| CI | None | GitHub Actions: Release and ASan+UBSan+LSan |

---

## Roadmap

- **Real-flow inputs:** use slang's `Driver` for filelists (`-f`), `+incdir`, `+define` and `--top`.
- **DV-oriented examples:** generate an instantiation or testbench skeleton from a module's ports, and report unconnected or undriven ports.
- **Packaging:** `pkgIndex.tcl` and a CMake install target, so `package require tclslang` works without `load`.
- **slang version:** move from a pinned commit to a tagged slang release.

---

## License

MIT. See [LICENSE](LICENSE).
