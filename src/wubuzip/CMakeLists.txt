# wubuzip: from-scratch ZIP + DEFLATE (reader and writer)
add_library(wubuzip STATIC
  crc.c zip.c reader.c bit.c bitw.c canon.c huffman.c fixed.c
  fixedcode.c limitcode.c lz77.c deflate.c block.c inflate.c)
target_include_directories(wubuzip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
