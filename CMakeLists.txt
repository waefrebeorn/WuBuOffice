cmake_minimum_required(VERSION 3.10)
project(WuBuOffice C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Expose POSIX functions (strdup, open_memstream) under -std=c11 so pointer-
# returning POSIX calls are not implicitly truncated to int.
add_compile_definitions(_POSIX_C_SOURCE=200809L)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# ccache: auto-activate if present (cache compiler launches -> faster rebuilds).
# Harmless no-op when ccache is not installed; install it with:
#   sudo apt-get install -y ccache
# and it is picked up on the next configure (also works with the Ninja
# generator, which is the recommended fast generator for this project).
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(STATUS "ccache: ${CCACHE_PROGRAM} (compiler launches cached)")
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CUDA_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Optional AddressSanitizer + UndefinedBehaviorSanitizer build. Enables the
# leak/UB gate used by the CI "sanitizer" job. Off by default so the normal
# build stays fast and dependency-free.
option(WITH_SANITIZER "Build with -fsanitize=address,undefined (leak/UB gate)" OFF)
if(WITH_SANITIZER)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1)
  add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
endif()

# Static single-binary build (#100): link every executable with -static so the
# produced image2doc / ocrserve / crnn_* tools are self-contained. Off by default.
option(STATIC "Build fully static executables (-static)" OFF)
if(STATIC)
  add_link_options(-static)
endif()

# Optional CUDA training backend (requires nvcc). Declared here so it is
# discoverable on the command line; the actual target lives in src/gpu/.
option(WITH_CUDA "Build the CUDA training backend (requires nvcc)" OFF)

# ws07#1339: this is a NATIVE office suite. Reject Electron / JVM / any
# bundled web runtime at configure time so a dependency can never silently
# pull one in. Native C11 + pthreads only.
if(DEFINED ENV{ELECTRON})
  message(FATAL_ERROR "WuBuOffice is native-only: ELECTRON is not allowed "
          "(ws07#1339). Build with plain CMake, no Electron toolchain.")
endif()
if(DEFINED ENV{WUBOFFICE_ALLOW_JVM})
  message(WARNING "WUBOFFICE_ALLOW_JVM set: linking a JVM is permitted only for "
          "explicit server-side tooling, never the document apps (ws07#1339).")
endif()
find_library(JVM_LIB jvm)
if(JVM_LIB AND NOT WUBOFFICE_ALLOW_JVM)
  message(FATAL_ERROR "libjvm found on the link path and WUBOFFICE_ALLOW_JVM is "
          "not set. WuBuOffice document apps must not link the JVM (ws07#1339). "
          "Set -DWUBOFFICE_ALLOW_JVM=ON only for server-side tooling.")
endif()

enable_testing()

# --- modules: each layer / module owns its own build file (no monolith) ---
add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(tools)
add_subdirectory(tests)

# Modular smoke target: run every test EXCEPT the slow OCR/CNN battery.
# `cmake --build build --target smoke` or `./tools/smoke.sh <module>`.
add_custom_target(smoke
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -LE ocr --output-on-failure
    COMMENT "smoke: all module tests except the slow ocr suite (use tools/smoke.sh <module> for one module)")
