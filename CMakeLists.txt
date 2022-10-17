cmake_minimum_required(VERSION 2.8)
project(finaltask)

find_package(Threads)
add_executable(finaltask task.cpp)
target_link_libraries(finaltask ev ${CMAKE_THREAD_LIBS_INIT})
