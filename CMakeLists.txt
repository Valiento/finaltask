cmake_minimum_required(VERSION 2.8)
project(final)

#set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

add_executable(final task.cpp)
#target_link_libraries(final PRIVATE pthread)
