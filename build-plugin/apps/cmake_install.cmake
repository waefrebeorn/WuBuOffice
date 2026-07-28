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
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
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
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuword/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuwordwin/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuos/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubucell/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubushow/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wuburead/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuedit/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubudoc/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuodf/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuconv/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubulegacy/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubupdf/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuoffice/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubusvg/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubufont/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuocr/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubugauntlet/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubunote/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuview/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubuwordview/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/wubupad_bridge/cmake_install.cmake")

endif()

