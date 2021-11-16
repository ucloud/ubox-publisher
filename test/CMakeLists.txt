cmake_minimum_required(VERSION 3.1)
add_executable(srv unixsocket_srv.cpp)
add_executable(cli unixsocket_cli.cpp)

include_directories(
	"${CMAKE_SOURCE_DIR}/inc"
	"${CMAKE_SOURCE_DIR}/third_party"
	"${CMAKE_SOURCE_DIR}/third_party/openssl-1.0.2h/include"
    /usr/include/opencv4
	)
add_executable(camera camera.cpp)
target_link_libraries(camera PRIVATE opencv_videoio opencv_imgproc opencv_core)
target_link_libraries(camera PRIVATE opencv_highgui)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2 -Wall")
