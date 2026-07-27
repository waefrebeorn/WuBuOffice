# wubupng: single correct PNG encoder (RGBA + GRAY8). Reuses zlib.
find_package(ZLIB REQUIRED)
add_library(wubupng STATIC wubupng.c wubupng.h)
target_include_directories(wubupng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubupng PUBLIC ZLIB::ZLIB)
target_compile_options(wubupng PRIVATE -Wall -Wextra -Wpedantic)
