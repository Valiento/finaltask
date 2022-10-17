cmake_minimum_required(VERSION 2.8)
project(finaltask)

find_package(Threads)
set(CMAKE_CXX_STANDARD 11)
add_executable(finaltask task.cpp)
target_link_libraries(finaltask ev ${CMAKE_THREAD_LIBS_INIT})
