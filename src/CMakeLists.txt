find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

include_directories(
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/third_party"
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)
link_directories(
    ${GLIB_LIBRARY_DIRS}
    ${GSTREAMER_LIBRARY_DIRS}
)

aux_source_directory(. PROJECT_SOURCES)
aux_source_directory(${CMAKE_SOURCE_DIR}/third_party/cjson CJSON_SRC)
aux_source_directory(${CMAKE_SOURCE_DIR}/third_party/tinylog TLOG_SRC)
add_executable(ubox-publisher ${PROJECT_SOURCES} ${CJSON_SRC} ${TLOG_SRC})

target_link_libraries(ubox-publisher pthread ${GSTREAMER_LIBRARIES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O2 -Wall")

install(TARGETS ubox-publisher
        RUNTIME DESTINATION bin
)
