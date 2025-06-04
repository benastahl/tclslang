# default path lol
TCL_SCRIPT_PATH ?= ./test.tcl

run:
	./build/tclslang

build:
	make clean
	cmake -B build -DCMAKE_POSITION_INDEPENDENT_CODE=ON
	cmake --build build -j$(nproc)
	tclsh ${TCL_SCRIPT_PATH}

clean:
	rm -rf build
