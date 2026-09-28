JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu)
IMAGE ?= tclslang

.PHONY: build test example clean docker docker-test

build:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j$(JOBS)

test: build
	ctest --test-dir build --output-on-failure

example: build
	tclsh examples/dump_hierarchy.tcl

clean:
	rm -rf build

docker:
	docker build -t $(IMAGE) .

docker-test: docker
	docker run --rm $(IMAGE)
