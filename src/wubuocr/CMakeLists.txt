# wubuocr: clean-room image -> structured document digestion
# Netpbm decode -> Otsu binarize -> XY-cut reading order -> connected components
# -> document JSON model. Dependency-free; recognizer is a pluggable slot.
add_library(wubuocr STATIC
    image.c binarize.c layout.c
    components.c wubuocr.c
    font8x8.c recognize.c
    fontbank.c gauntlet.c
    page_compose.c latin1.c
    unicode.c fontdir.c
    zoning.c mlp.c
    convnet.c conv_gemm.c convnet3.c convnet3_cbam.c
    stylebank.c dft.c goldplace.c
    crnn_transcribe.c crnn_transcribe_pre.c docfmt.c pdfsearch.c
    qr.c
    gru_layout.c)
target_include_directories(wubuocr PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubujson
    ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(wubuocr PUBLIC wubujson wubufont wubuzip wubupng crnnocr m)

# --- Modern CRNN + CTC modular OCR pipeline (clean-room, dependency-free) ---
# Self-contained modules, opaque structs, no god headers:
#   crnn.c   sequence model (conv trunk + recurrent + CTC head), save/load
#   ctc.c    CTC loss (label-smooth + focal) + Graves two-score beam decode
#   rnn.c    LSTM core          gru.c  GRU core (extracted, single offset table)
#   lexicon.c  beam post-correction
#   lens.c   perspective flatten (Office-Lens corner warp)
#   png.c / png_encode.c  no-lib PNG decode/encode (reuses wubuzip inflate)
# The conv trunk + image plumbing are reused from the wubuocr lib
# (convnet3.c / image.c / mlp.c). Each module compiles and links independently.
add_library(crnnocr STATIC
    crnn.c
    ctc.c
    rnn.c
    $<$<BOOL:${WITH_CUDA}>:${CMAKE_SOURCE_DIR}/src/gpu/gru_gpu.c>
    gru.c
    ocl_gru.c
    lexicon.c
    lens.c
    png.c
    png_encode.c)
target_include_directories(crnnocr PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubufont
    ${CMAKE_SOURCE_DIR}/src/wubujson
    ${CMAKE_SOURCE_DIR}/src/wubuzip)
target_link_libraries(crnnocr PUBLIC wubuocr dl)
if(WITH_CUDA)
    target_link_libraries(crnnocr PUBLIC gpublas)
    # when CUDA is on, use the GPU-matmul GRU instead of the scalar one
    set_source_files_properties(gru.c PROPERTIES HEADER_FILE_ONLY ON)
    target_compile_definitions(crnnocr PUBLIC WITH_CUDA_BUILD)
endif()
