SET(TARGET gstuclockoverlay)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERBASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(PANGO REQUIRED pango)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)

include_directories(
    ${GSTREAMERBASE_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
    ${PANGOCAIRO_INCLUDE_DIRS}
)
link_directories(
    ${GSTREAMERBASE_LIBRARY_DIRS}
    ${CAIRO_LIBRARY_DIRS}
    ${PANGO_LIBRARY_DIRS}
    ${PANGOCAIRO_LIBRARY_DIRS}
)

aux_source_directory(. PROJECT_SOURCES)
add_library(${TARGET} SHARED ${PROJECT_SOURCES})

target_link_libraries(${TARGET} pthread ${GSTREAMERBASE_LIBRARIES} ${GSTREAMERVIDEO_LIBRARIES} ${PANGO_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGOCAIRO_LIBRARIES})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall")

pkg_get_variable(GSTREAMER_PLUGIN_DIR gstreamer-1.0 pluginsdir)

message("`make install` install ${TARGET} to ${GSTREAMER_PLUGIN_DIR}")
set(EXP_GSTREAMER_PLUGIN_DIR ${GSTREAMER_PLUGIN_DIR} PARENT_SCOPE)

install(TARGETS ${TARGET}
    DESTINATION ${GSTREAMER_PLUGIN_DIR}
)
