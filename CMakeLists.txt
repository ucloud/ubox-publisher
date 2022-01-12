cmake_minimum_required(VERSION 3.4)
project(publisher C CXX)
add_subdirectory(src bin)
add_subdirectory(wrhcamerasrc)
add_subdirectory(uv4l2src)
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

set(CPACK_PACKAGE_NAME                      "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION                   "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY       "publisher")
set(CPACK_SYSTEM_NAME                       "${ARCH}")
set(CPACK_PACKAGE_CONTACT                   "Faicker Mo <faicker.mo@ucloud.cn>")

if (${LINUXOS} STREQUAL "centos") #centos 8
  SET(CPACK_GENERATOR                         "RPM")
  set(CPACK_RPM_PACKAGE_ARCHITECTURE          "${ARCH}")
  set(CPACK_RPM_FILE_NAME                     "RPM-DEFAULT")
  set(CPACK_RPM_PACKAGE_RELEASE_DIST          1)
  set(CPACK_RPM_PACKAGE_REQUIRES              "gstreamer1 glib2")
  set(CPACK_RPM_DEBUGINFO_PACKAGE             1)
else()
  set(CPACK_GENERATOR "DEB")
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgstreamer1.0-0, libglib2.0-0")  #ubuntu 18.04
endif ()

include(CPack)
