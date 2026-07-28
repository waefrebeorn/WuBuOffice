# wubupad_bridge: cross-repo reuse of WuBuPad's from-scratch editor core.
# Locate the sibling WuBuPad build (../WuBuPad/build) and its headers
# (../WuBuPad/src). If WuBuPad has been built, this gives WuBuOffice a fast,
# tested editing engine (Doc + DONE search) without duplicating it.

find_library(WUBUPAD_UI_LIB     NAMES wubupad_ui    PATHS ${CMAKE_SOURCE_DIR}/../WuBuPad/build NO_DEFAULT_PATH)
find_library(WUBUPAD_CORE_LIB   NAMES wubupad_core  PATHS ${CMAKE_SOURCE_DIR}/../WuBuPad/build NO_DEFAULT_PATH)
find_library(WUBUPAD_ATOM_LIB   NAMES wubupad_atom  PATHS ${CMAKE_SOURCE_DIR}/../WuBuPad/build NO_DEFAULT_PATH)
find_path(WUBUPAD_INC          NAMES doc.h         PATHS ${CMAKE_SOURCE_DIR}/../WuBuPad/src NO_DEFAULT_PATH)

if(NOT WUBUPAD_UI_LIB OR NOT WUBUPAD_CORE_LIB OR NOT WUBUPAD_ATOM_LIB OR NOT WUBUPAD_INC)
  message(STATUS "wubupad_bridge: WuBuPad not built at ../WuBuPad/build -- bridge DISABLED (run WuBuPad build first).")
  return()
endif()

message(STATUS "wubupad_bridge: reusing WuBuPad core at ${WUBUPAD_CORE_LIB}")

# Reuse WuBuPad's from-scratch editor core. We import the built static
# archives as IMPORTED targets so CMake resolves their transitive link
# dependencies (ui -> atom -> core) and orders the archives correctly --
# linking raw .a paths drops symbols (classic archive-ordering trap).
add_library(wubupad_core_imp STATIC IMPORTED)
set_target_properties(wubupad_core_imp PROPERTIES
    IMPORTED_LOCATION ${WUBUPAD_CORE_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${WUBUPAD_INC})
add_library(wubupad_atom_imp STATIC IMPORTED)
set_target_properties(wubupad_atom_imp PROPERTIES
    IMPORTED_LOCATION ${WUBUPAD_ATOM_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${WUBUPAD_INC})
add_library(wubupad_ui_imp STATIC IMPORTED)
set_target_properties(wubupad_ui_imp PROPERTIES
    IMPORTED_LOCATION ${WUBUPAD_UI_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${WUBUPAD_INC})

add_library(wubupad_bridge STATIC wubupad_bridge.c)
target_include_directories(wubupad_bridge PUBLIC
    ${WUBUPAD_INC}
    ${CMAKE_CURRENT_SOURCE_DIR})
# wubupad_ui (headless UI) depends on wubupad_atom (command palette / Atom
# spine) which depends on wubupad_core (Doc/session model + json). Linking the
# imported targets (not raw paths) lets CMake emit the correct archive order so
# every transitive symbol (ui_atom -> pkgmgr -> j_parse, etc.) resolves.
target_link_libraries(wubupad_bridge PUBLIC
    wubupad_ui_imp wubupad_atom_imp wubupad_core_imp)

add_executable(wubupad_edit wubupad_edit.c)
target_link_libraries(wubupad_edit PRIVATE wubupad_bridge)

# Bridge self-test (only built when the bridge is enabled).
add_executable(test_wubupad_bridge test_wubupad_bridge.c)
target_link_libraries(test_wubupad_bridge PRIVATE wubupad_bridge)
add_test(NAME wubupad_bridge COMMAND test_wubupad_bridge)
