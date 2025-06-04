# Tclslang

---

Tclslang is a Tcl extension that provides a programmable interface for analyzing Verilog hardware modules using [Slang](https://github.com/MikePopoloski/slang) as a backend. It allows Verilog parsing and structured introspection of modules, ports, cells, and drivers directly from Tcl scripts.

Developed by **Ben Stahl**  
GitHub: [github.com/benastahl/tclslang](https://github.com/benastahl/tclslang)

---


# Installing Slang on RedHat for Shared Objects

## Dependencies

---

- [Slang Verilog Compiler](https://github.com/MikePopoloski/slang)

- Tcl (>= 8.1)

---

### 1. Install Development Tools
`sudo yum group install "Development Tools"`

### 2. Install CMake
`sudo yum install cmake`

### 3. Clone the slang repository
`git clone https://github.com/MikePopoloski/slang.git`  
`cd slang`

### 4. Install Python's package manager (pip)
`sudo yum install python3-pip`

### 5. Install Conan for dependency management
`pip3 install conan`

### 6. Use Conan to install dependencies
`conan install . -b missing -o fmt/*:shared=True`

### 7. Configure the build for shared object (.so) with CMake
`cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
`
### 8. Build the project
`cmake --build build -j$(nproc)
`
### 9. (Optional) Run tests to verify the build
`cd build`  
`ctest --output-on-failure`

`cd ..` (cd slang dir)

### 10. Install the shared library to your system
`sudo cmake --install build --strip`

`cd ..` (to main dir)  
`make clean`  
`make build TCL_SCRIPT_PATH=./yerrr.tcl`

---

## Features

---

- Parses Verilog files and builds a syntax tree
- Exposes structural elements as Tcl commands:
  - `tree`, `module`, `port`, `cell`, `connection`, `driver`
- Navigate HDL hierarchies using commands like:
  - `get_module`, `get_ports`, `get_cells`, `get_driver`
- Driver objects support types: `const`, `var`, and `net`
- Dynamically registers commands based on parsed structure

---

## Quick Usage Example

---

```tcl
set tree [slang_parse foo.sv]
set module [$tree get_module top]
set ports [$module get_ports]
set port [lindex $ports 0]
$port name
````

---

## Object Interface Overview

---

### 🔷 Tree Object

Created by parsing Verilog files:

```tcl
set tree [slang_parse file1.sv file2.sv ...]
```

**Methods:**

```tcl
$tree get_module <module_name>
```

---

### 🔷 Module Object

Created via `$tree get_module`.

**Methods:**

```tcl
$module name
$module get_ports
$module get_cells
```

---

### 🔷 Port Object

Returned by `$module get_ports` or `$cell get_ports`.

**Methods:**

```tcl
$port name          ;# Port name
$port type          ;# Data type
$port direction     ;# Input/output/inout
$port portType      ;# e.g., "modport", "interface"
$port dimType       ;# Dimension type info
$port dimensions    ;# List of index pairs (e.g., {{3 0}})
```

---

### 🔷 Cell Object

Returned by `$module get_cells` or `$cell get_cells`.

**Methods:**

```tcl
$cell name
$cell get_ports
$cell get_connections
$cell get_cells     ;# For hierarchical cells
```

---

### 🔷 Connection Object

Returned by `$cell get_connections`.

**Methods:**

```tcl
$conn name           ;# Port name being connected
$conn get_port       ;# Returns a port object
$conn get_driver     ;# Returns a driver object
```

---

### 🔷 Driver Object

Returned by `$conn get_driver`.
Use `$driver type` to determine subtype: `const`, `var`, or `net`.

---

#### ✅ Common Methods (All Drivers)

```tcl
$driver name         ;# Identifier name (not for const)
$driver type         ;# "const", "var", or "net"
```

---

#### 🔸 Const Driver

```tcl
$driver const        ;# Returns constant value (e.g., 1'b1)
```

---

#### 🔸 Var Driver

```tcl
$driver data_type    ;# Returns Verilog data type (e.g., logic [3:0])
```

---

#### 🔸 Net Driver

```tcl
$driver data_type    ;# Same as for var
$driver net_type     ;# e.g., "wire", "tri"
```

---

## Build & Install

---

**Dependencies:**

* [Slang Verilog Compiler](https://github.com/MikePopoloski/slang)
* Tcl (≥ 8.1)

**Build Instructions:**

```bash
mkdir build && cd build
cmake ..
make
make install
```

---

## Tcl Integration

---

```tcl
load ./libtclslang.so
package require tclslang
```

---

## License

---

MIT License. See `LICENSE` file for details.

```
