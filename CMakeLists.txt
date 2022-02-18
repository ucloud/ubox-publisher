cmake_minimum_required(VERSION 3.4)
project(ubox-publisher C CXX)
add_subdirectory(src bin)
if (EXISTS ${CMAKE_SOURCE_DIR}/wrhcamerasrc/CMakeLists.txt)
    add_subdirectory(wrhcamerasrc)
endif ()
add_subdirectory(uv4l2src)
add_subdirectory(uclockoverlay)
add_subdirectory(uflv)
add_subdirectory(test)

execute_process(
    COMMAND bash -c "git describe --tags 2> /dev/null || echo `git rev-parse --short HEAD`"
    OUTPUT_VARIABLE PROJECT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
execute_process(
    COMMAND bash -c "uname -m"
    OUTPUT_VARIABLE ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

execute_process(
    COMMAND bash -c "awk -F= '/^ID=/{print $2}' /etc/os-release |tr -d '\n' | tr -d '\"'"
    OUTPUT_VARIABLE LINUXOS
    )

execute_process(
    COMMAND bash -c "awk -F= '/^VERSION_ID=/{print $2}' /etc/os-release |tr -d '\n' | tr -d '\"'"
    OUTPUT_VARIABLE LINUXOSVERSION
    )

set(CPACK_PACKAGE_NAME                      "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION                   "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY       "ubox-publisher")
set(CPACK_SYSTEM_NAME                       "${ARCH}")
set(CPACK_PACKAGE_CONTACT                   "Faicker Mo <faicker.mo@ucloud.cn>")

if (${LINUXOS} STREQUAL "centos") #centos 7/8
    SET(RTMPVERSION "2.4-16.el8")
    if (${LINUXOSVERSION} STREQUAL "7")
        SET(RTMPVERSION "2.4-16.el7")
    endif ()

    SET(CPACK_GENERATOR                         "RPM")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE          "${ARCH}")
    set(CPACK_RPM_FILE_NAME                     "RPM-DEFAULT")
    set(CPACK_RPM_PACKAGE_RELEASE_DIST          1)
    set(CPACK_RPM_PACKAGE_REQUIRES               "pango, gstreamer1, glib2, gstreamer1-vaapi, gstreamer1-libav, gstreamer1-plugins-base, gstreamer1-plugins-bad-free, gstreamer1-plugins-good, gstreamer1-plugins-ugly, gstreamer1-plugins-ugly-free, libva-intel-driver, gstreamer1-plugins-bad-freeworld, librtmp == ${RTMPVERSION}")
    set(CPACK_RPM_DEBUGINFO_PACKAGE             1)
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION    "${EXP_GSTREAMER_PLUGIN_DIR}")
else()
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgstreamer1.0-0, libglib2.0-0, libpango-1.0-0, gstreamer1.0-plugins-ugly, gstreamer1.0-plugins-good, gstreamer1.0-plugins-base, gstreamer1.0-plugins-bad, gstreamer1.0-tools, gstreamer1.0-rtsp, gstreamer1.0-vaapi, gstreamer1.0-libav, librtmp1 (== 2.4+20151223.gitfa8646d.1-2)")  #ubuntu 18.04/20.04
endif ()

include(CPack)
