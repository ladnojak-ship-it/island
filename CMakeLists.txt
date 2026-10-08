cmake_minimum_required(VERSION 3.16)
project(DynamicIsland CXX)
set(CMAKE_CXX_STANDARD 17)
file(GLOB SRC src/*.cpp)
add_executable(DynamicIsland WIN32 ${SRC})
target_compile_definitions(DynamicIsland PRIVATE UNICODE _UNICODE)
target_compile_options(DynamicIsland PRIVATE /utf-8 /EHsc /O2)
# библиотеки подключаются через #pragma comment(lib, ...) в main.cpp
