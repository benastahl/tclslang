run:
	./build/tclslang

build:
	cmake -B build -DCMAKE_POSITION_INDEPENDENT_CODE=ON
	cmake --build build -j$(nproc)
	tclsh ./test.tcl

clean:
	rm -rf build
