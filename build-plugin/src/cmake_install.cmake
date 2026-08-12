# Install script for directory: /home/wubu/WuBuOffice/src

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
  include("/home/wubu/WuBuOffice/build-plugin/src/wububase/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubupng/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wuburender/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuzip/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuxml/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubucfb/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuformula/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuspell/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuchart/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuautosave/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubudraw/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubumath/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuepub/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubua11y/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuscript/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubumodel/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuoxml/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubujson/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubusvg/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubufont/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuocr/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuimage/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubutui/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubunote/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubudoc/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/gpu/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubusettings/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubushape/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubulayout/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuexp/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubutoc/cmake_install.cmake")

endif()

