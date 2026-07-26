# wubupad_bridge: cross-repo reuse of WuBuPad's from-scratch editor core.
# Locate the sibling WuBuPad build (../WuBuPad/build) and its headers
# (../WuBuPad/src). If WuBuPad has been built, this gives WuBuOffice a fast,
# tested editing engine (Doc + DONE search) without duplicating it.

find_library(WUBUPAD_UI_LIB     NAMES wubupad_ui    PATHS ${CMAKE_SOURCE_DIR}/../WuBuPad/build NO_DEFAULT_PATH)
find_library(WUBUPAD_CORE_LIB   NAMES wubupad_core  PATHS ${CMAKE_SOURCE_DIR}/../WuBuPad/build NO_DEFAULT_PATH)
find_path(WUBUPAD_INC          NAMES doc.h         PATHS ${CMAKE_SOURCE_DIR}/../WuBuPad/src NO_DEFAULT_PATH)

if(NOT WUBUPAD_UI_LIB OR NOT WUBUPAD_CORE_LIB OR NOT WUBUPAD_INC)
  message(STATUS "wubupad_bridge: WuBuPad not built at ../WuBuPad/build -- bridge DISABLED (run WuBuPad build first).")
  return()
endif()

message(STATUS "wubupad_bridge: reusing WuBuPad core at ${WUBUPAD_CORE_LIB}")

add_library(wubupad_bridge STATIC wubupad_bridge.c)
target_include_directories(wubupad_bridge PUBLIC
    ${WUBUPAD_INC}
    ${CMAKE_CURRENT_SOURCE_DIR})
# wubupad_ui bundles the SDL2/Freetype gfx backend; resolve those symbols so
# the headless-only bridge links. They are installed (via rootexec) on the
# dev box; if absent, the bridge still links for any app that doesn't touch gfx.
find_package(SDL2)
find_package(Freetype)
set(_wubupad_extra "")
if(SDL2_FOUND)
  list(APPEND _wubupad_extra SDL2::SDL2)
endif()
if(Freetype_FOUND)
  list(APPEND _wubupad_extra Freetype::Freetype)
endif()
# wubupad_ui is the headless-only UI lib (no SDL2/Freetype); it depends on
# wubupad_core for the Doc/session model. Link UI before core so the archive
# order resolves (UI references core symbols). wubupad_gfx (SDL2) is separate
# and not needed here.
target_link_libraries(wubupad_bridge PUBLIC ${WUBUPAD_UI_LIB} ${WUBUPAD_CORE_LIB})

add_executable(wubupad_edit wubupad_edit.c)
target_link_libraries(wubupad_edit PRIVATE wubupad_bridge)

# Bridge self-test (only built when the bridge is enabled).
add_executable(test_wubupad_bridge test_wubupad_bridge.c)
target_link_libraries(test_wubupad_bridge PRIVATE wubupad_bridge)
add_test(NAME wubupad_bridge COMMAND test_wubupad_bridge)
