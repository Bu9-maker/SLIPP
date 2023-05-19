project(slipp)
cmake_minimum_required(VERSION 2.8)

add_compile_options(--std=c++11)

include_directories(src/core)

add_executable(test src/example/test.cpp)
add_executable(test_train src/example/test_train.cpp)
add_executable(example src/example/example.cpp)
