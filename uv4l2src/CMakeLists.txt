SET(TARGET gstuv4l2src)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)

include_directories(
    ${GSTREAMERBASE_INCLUDE_DIRS}
)
link_directories(
    ${GSTREAMERBASE_LIBRARY_DIRS}
)

aux_source_directory(. PROJECT_SOURCES)
add_library(${TARGET} SHARED ${PROJECT_SOURCES})

target_link_libraries(${TARGET} pthread ${GSTREAMERBASE_LIBRARIES} ${GSTREAMERVIDEO_LIBRARIES})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall")

pkg_get_variable(GSTREAMER_PLUGIN_DIR gstreamer-1.0 pluginsdir)

message("install ${TARGET} to ${GSTREAMER_PLUGIN_DIR}")

install(TARGETS ${TARGET}
    DESTINATION ${GSTREAMER_PLUGIN_DIR}
)
