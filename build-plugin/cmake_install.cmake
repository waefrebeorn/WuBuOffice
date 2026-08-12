# Install script for directory: /home/wubu/WuBuOffice

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
  include("/home/wubu/WuBuOffice/build-plugin/src/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubucrdt/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuhistory/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubusync/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubucol/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuvars/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubulang/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubucaption/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuwatermark/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubufocus/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubudyslexia/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuscope/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubua11ytree/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubueqnum/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuheading/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubucite/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuhash/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubusig/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wuburedact/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubucsv/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubupasteplain/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wuburtf/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubua11yannounce/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuform/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuexp_png/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubupdfextract/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuxps/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubuaislot/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubusandbox/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubufmtpaint/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubunesttab/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/src/wubupdfform/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/apps/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/tools/cmake_install.cmake")
  include("/home/wubu/WuBuOffice/build-plugin/tests/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/wubu/WuBuOffice/build-plugin/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
