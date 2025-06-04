# Installing Slang on RedHat for Shared Objects

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
`make build`

# USAGE



