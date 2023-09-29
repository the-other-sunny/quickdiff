cmake_minimum_required(VERSION 3.22)
project(quickdiff)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

## Fetch dependencies

include(FetchContent)

FetchContent_Declare(
    difflib
    GIT_REPOSITORY https://github.com/duckie/difflib.git
)
FetchContent_GetProperties(difflib)
if(NOT difflib_POPULATED)
    FetchContent_Populate(difflib)
endif()
add_library(difflib INTERFACE)
target_include_directories(difflib INTERFACE ${difflib_SOURCE_DIR}/src)

FetchContent_Declare(
    utfcpp
    GIT_REPOSITORY https://github.com/nemtrif/utfcpp
)
FetchContent_MakeAvailable(utfcpp)

##

add_executable(quickdiff src/quickdiff.cpp)

include(FindOpenMP)
if(OpenMP_CXX_FOUND) # TODO: handle the case in which CMake couldn't find OpenMP
    target_link_libraries(quickdiff PRIVATE OpenMP::OpenMP_CXX)
endif()

target_link_libraries(quickdiff PRIVATE difflib utf8::cpp)
