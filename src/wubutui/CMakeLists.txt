# wubutui: native terminal UI toolkit (human GUIs)
# Pure C11 render/input core (screen buffer, ANSI diff, key decode, word wrap,
# draw primitives) + a thin POSIX termios edge (term.c). No dependency, no
# toolkit -- consistent with the headless suite. The pure modules are unit-
# tested without a TTY; term.c isolates the impure edge.
add_library(wubutui STATIC
    screen.c input.c
    draw.c   term.c)
target_include_directories(wubutui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
