cmake_minimum_required(VERSION 3.1)
add_executable(cli unixsocket_cli.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2 -Wall")
