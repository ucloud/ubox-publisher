SET(TARGET gstuflv)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GSTREAMERAUDIO REQUIRED gstreamer-audio-1.0)
pkg_check_modules(GSTREAMERPBUTIL REQUIRED gstreamer-pbutils-1.0)
pkg_check_modules(GSTREAMERTAG REQUIRED gstreamer-tag-1.0)

include_directories(
    ${GSTREAMERBASE_INCLUDE_DIRS}
)
link_directories(
    ${GSTREAMERBASE_LIBRARY_DIRS}
)

aux_source_directory(. PROJECT_SOURCES)
list(REMOVE_ITEM PROJECT_SOURCES "./gstmemindex.c")
list(REMOVE_ITEM PROJECT_SOURCES "./gstindex.c")

add_library(${TARGET} SHARED ${PROJECT_SOURCES})

target_link_libraries(${TARGET} pthread ${GSTREAMERBASE_LIBRARIES} ${GSTREAMERVIDEO_LIBRARIES}
    ${GSTREAMERAUDIO_LIBRARIES} ${GSTREAMERPBUTIL_LIBRARIES} ${GSTREAMERTAG_LIBRARIES})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall")

pkg_get_variable(GSTREAMER_PLUGIN_DIR gstreamer-1.0 pluginsdir)

message("`make install` install ${TARGET} to ${GSTREAMER_PLUGIN_DIR}")
set(EXP_GSTREAMER_PLUGIN_DIR ${GSTREAMER_PLUGIN_DIR} PARENT_SCOPE)

install(TARGETS ${TARGET}
    DESTINATION ${GSTREAMER_PLUGIN_DIR}
)
