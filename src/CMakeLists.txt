include_directories(
	"${CMAKE_SOURCE_DIR}/inc"
	"${CMAKE_SOURCE_DIR}/third_party"
    "/usr/include/opencv4"
	"/usr/include/ffmpeg"
	)
link_directories(
)
aux_source_directory(. PROJECT_SOURCES)
aux_source_directory(${CMAKE_SOURCE_DIR}/third_party/cjson CJSON_SRC)
aux_source_directory(${CMAKE_SOURCE_DIR}/third_party/tinylog TLOG_SRC)
add_executable(publisher ${PROJECT_SOURCES} ${CJSON_SRC} ${TLOG_SRC})

target_link_libraries(publisher -lrtmp )
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
	target_link_libraries(publisher asound)
	message(status "current platform:linux")
endif ()
target_link_libraries(publisher faac)
target_link_libraries(publisher pthread)
target_link_libraries(publisher avformat avcodec)
target_link_libraries(publisher opencv_videoio opencv_imgproc opencv_core opencv_highgui)
target_link_libraries(publisher x264)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2 -Wall")
