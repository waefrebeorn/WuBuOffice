# wubuimage: clean-room raster decoders (PNG; no libpng)
# PNG decode reuses the in-tree wubuzip DEFLATE engine; outputs an RGBA plane.
# img2doc bridges a decoded raster into the wubuocr pipeline (image -> doc).
# pdf_extract pulls text out of dropped PDFs (no third-party lib; FlateDecode
#   via wubuzip). All three are the "drag a document/image in" front-end.
add_library(wubuimage STATIC
    png.c img2doc.c imgops.c
    ${CMAKE_SOURCE_DIR}/src/wubupdf/pdf_extract.c)
target_include_directories(wubuimage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubuzip
    ${CMAKE_SOURCE_DIR}/src/wubuocr
    ${CMAKE_SOURCE_DIR}/src/wubufont
    ${CMAKE_SOURCE_DIR}/src/wubupdf)
target_link_libraries(wubuimage PUBLIC wubuzip wubuocr wubufont m)
