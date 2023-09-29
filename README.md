# quickdiff

## Build

### GCC

```sh
mkdir build
cd build
C=gcc CXX=g++ CMAKE_BUILD_TYPE=Release cmake ..
cmake --build .
```

### Clang

```sh
apt install clang libomp-dev
```

``` sh
mkdir build
cd build
C=clang CXX=clang++ CMAKE_BUILD_TYPE=Release cmake ..
cmake --build .
```

## Usage

```sh
cat order.bin | quickdiff > result.bin
```
