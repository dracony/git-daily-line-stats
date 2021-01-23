cmake_minimum_required(VERSION 3.16)
project(codestats)

set(CMAKE_CXX_STANDARD 20)

set(CONAN_CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(codestats main.cpp)
target_link_libraries(codestats ${CONAN_LIBS})