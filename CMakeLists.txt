cmake_minimum_required(VERSION 3.10)
project(WuBuOffice C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Expose POSIX functions (strdup, open_memstream) without relying on GNU
# extensions — every TU needs this so pointer-returning POSIX calls are not
# implicitly truncated to int under -std=c11.
add_compile_definitions(_POSIX_C_SOURCE=200809L)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

# --- libraries ---
add_library(wubuzip STATIC src/wubuzip/zip.c)
target_include_directories(wubuzip PUBLIC src/wubuzip)

add_library(wubuxml STATIC src/wubuxml/xml.c)
target_include_directories(wubuxml PUBLIC src/wubuxml)

add_library(wubuoxml STATIC src/wubuoxml/package.c)
target_include_directories(wubuoxml PUBLIC src/wubuoxml)
target_link_libraries(wubuoxml PUBLIC wubuzip)  # package.c includes wubuzip/zip.h

# --- apps ---
add_executable(wubuword apps/wubuword/main.c)
target_link_libraries(wubuword PRIVATE wubuoxml wubuxml)
# wubuword's main.c includes headers from wubuoxml & wubuxml dirs:
target_include_directories(wubuword PRIVATE
  src/wubuoxml src/wubuxml)

# --- tests ---
enable_testing()
add_executable(test_wubuzip tests/test_wubuzip.c)
target_link_libraries(test_wubuzip PRIVATE wubuzip)
add_test(NAME wubuzip_crc COMMAND test_wubuzip)
