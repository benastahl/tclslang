# tclslang build environment on a RHEL-compatible base (Rocky Linux 9).
#
#   docker build -t tclslang .
#   docker run --rm tclslang                              # runs the test suite
#   docker run --rm -it -v "$PWD":/work tclslang bash     # dev shell on your checkout

FROM rockylinux:9

ARG FMT_VERSION=11.1.4
ARG SLANG_SHA=652a9ab5b4d093ff4f8fcf6e1e3fdcc7e292f931
ARG JOBS=4

RUN dnf install -y dnf-plugins-core \
 && dnf config-manager --set-enabled crb \
 && dnf install -y gcc-c++ make cmake git python3 tcl tcl-devel pkgconf-pkg-config diffutils \
      libasan libubsan \
 && dnf clean all

# fmt: installed as a shared library so both slang and tclslang link the same one
RUN git clone --depth 1 --branch ${FMT_VERSION} https://github.com/fmtlib/fmt.git /tmp/fmt \
 && cmake -S /tmp/fmt -B /tmp/fmt/build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON -DFMT_TEST=OFF -DFMT_DOC=OFF \
 && cmake --build /tmp/fmt/build -j${JOBS} \
 && cmake --install /tmp/fmt/build \
 && rm -rf /tmp/fmt

# slang: pinned to the commit tclslang was developed against
RUN git clone https://github.com/MikePopoloski/slang.git /tmp/slang \
 && git -C /tmp/slang checkout ${SLANG_SHA} \
 && cmake -S /tmp/slang -B /tmp/slang/build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DSLANG_INCLUDE_TESTS=OFF -DSLANG_INCLUDE_TOOLS=OFF -DSLANG_USE_MIMALLOC=OFF \
 && cmake --build /tmp/slang/build -j${JOBS} \
 && cmake --install /tmp/slang/build --strip \
 && rm -rf /tmp/slang

# /usr/local/lib64 is not on the default loader path on EL
RUN echo /usr/local/lib64 > /etc/ld.so.conf.d/local.conf && ldconfig

WORKDIR /work
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j${JOBS}

CMD ["ctest", "--test-dir", "build", "--output-on-failure"]
