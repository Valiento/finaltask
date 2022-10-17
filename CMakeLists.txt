cmake_minimum_required(VERSION 2.8)
project(finaltask)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

add_executable(finaltask task.cpp)
target_link_libraries(finaltask PRIVATE pthread)
