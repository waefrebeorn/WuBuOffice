# Install script for directory: /home/wubu/WuBuOffice/apps

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuword/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuwordwin/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuos/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubucell/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubushow/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wuburead/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuedit/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubudoc/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuodf/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuconv/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubulegacy/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubupdf/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuoffice/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubusvg/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubufont/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuocr/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubugauntlet/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubunote/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuview/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubuwordview/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-asan2/apps/wubupad_bridge/cmake_install.cmake")

endif()

