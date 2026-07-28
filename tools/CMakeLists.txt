# WuBuOffice tools/ executables. Mirrors the original monolith target set
# exactly (no new targets invented). Each links the modular libs from src/ + apps/.

# --- CRNN pipeline unit tests ---
add_executable(ctc_test ctc_test.c ${CMAKE_SOURCE_DIR}/src/wubuocr/ctc.c)
target_include_directories(ctc_test PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(ctc_test PRIVATE m)
add_test(NAME ctc_test COMMAND ctc_test)

add_executable(gru_test gru_test.c)
target_include_directories(gru_test PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
# Pull the active GRU/LSTM from crnnocr (under WITH_CUDA that is the GPU
# matmul GRU; otherwise the scalar one). Do NOT compile gru.c/rnn.c inline
# here -- with WITH_CUDA=ON those sources are HEADER_FILE_ONLY and would
# yield no object (undefined gru_create at link).
target_link_libraries(gru_test PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME gru_test COMMAND gru_test)

# OpenCL GRU fallback (#98): numeric equivalence to the scalar CPU forward.
add_executable(ocl_gru_test ocl_gru_test.c ${CMAKE_SOURCE_DIR}/src/wubuocr/ocl_gru.c)
target_include_directories(ocl_gru_test PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(ocl_gru_test PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME ocl_gru COMMAND ocl_gru_test)

# Math/equation line detector (#48).
add_executable(test_math test_math.c)
target_include_directories(test_math PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_math PRIVATE wubuocr m)
add_test(NAME test_math COMMAND test_math)

# QR-code codec (#49): encode -> matrix -> decode round-trip, plus image acquire.
add_executable(test_qr test_qr.c)
target_include_directories(test_qr PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_qr PRIVATE wubuocr m)
add_test(NAME test_qr COMMAND test_qr)

add_executable(rnn_test rnn_test.c)
target_include_directories(rnn_test PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(rnn_test PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME rnn_test COMMAND rnn_test)

add_executable(gradcheck_conv3 gradcheck_conv3.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c)
target_include_directories(gradcheck_conv3 PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(gradcheck_conv3 PRIVATE m)
add_test(NAME gradcheck_conv3 COMMAND gradcheck_conv3)

# CRNN training / inference tools (render glyphs -> train -> recognize).
foreach(t crnn_train crnn_img_train crnn_ocr_train crnn_lex_train crnn_warp_train)
  add_executable(${t} ${t}.c)
  target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
  target_link_libraries(${t} PRIVATE crnnocr wubufont m)
endforeach()
add_executable(crnn_doc_demo crnn_doc_demo.c)
target_include_directories(crnn_doc_demo PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(crnn_doc_demo PRIVATE crnnocr wubufont m)
add_executable(crnn_photo_demo crnn_photo_demo.c)
target_include_directories(crnn_photo_demo PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(crnn_photo_demo PRIVATE crnnocr wubufont m)

# image2doc: page image -> editable document via the CRNN line recognizer.
# PNG -> OcrImage -> crnn_transcribe (docmodel JSON) -> wubuconv (docx/md/...).
add_executable(image2doc image2doc.c)
target_include_directories(image2doc PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuocr
    ${CMAKE_SOURCE_DIR}/src/wubuimage
    ${CMAKE_SOURCE_DIR}/src/wubufont
    ${CMAKE_SOURCE_DIR}/apps/wubuconv)
target_link_libraries(image2doc PRIVATE crnnocr wubuimage wubuconv wubujson wubufont pthread m)

# gen_pgmpage: synthetic multi-line Latin page (PGM) for image2doc testing.
add_executable(gen_pgmpage gen_pgmpage.c)
target_include_directories(gen_pgmpage PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(gen_pgmpage PRIVATE wubuocr wubufont m)

# Synthetic skewed+noisy PNG page (photo-like validation input)
add_executable(gen_realpage gen_realpage.c)
target_include_directories(gen_realpage PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(gen_realpage PRIVATE crnnocr wubuimage wubufont m)

# crnn_save/crnn_load fidelity probe
add_executable(crnn_roundtrip crnn_roundtrip.c)
target_include_directories(crnn_roundtrip PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(crnn_roundtrip PRIVATE crnnocr wubufont m)
add_executable(fontcpgrep fontcpgrep.c)
target_include_directories(fontcpgrep PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(fontcpgrep PRIVATE crnnocr wubufont m)
add_executable(charsetgen charsetgen.c)
target_include_directories(charsetgen PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(charsetgen PRIVATE crnnocr wubufont m)

# ocringest: end-to-end coordinate-aware OCR ingestion demo.
add_executable(ocringest ocringest.c)
target_include_directories(ocringest PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(ocringest PRIVATE wubuocr wubufont wubujson wubuzip m)

# wubuocr: zoning + mlp module unit tests (dependency-free, fast).
add_executable(test_zoning ${CMAKE_SOURCE_DIR}/tests/test_zoning.c ${CMAKE_SOURCE_DIR}/src/wubuocr/zoning.c)
target_include_directories(test_zoning PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
add_test(NAME zoning COMMAND test_zoning)

add_executable(test_mlp ${CMAKE_SOURCE_DIR}/tests/test_mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c)
target_include_directories(test_mlp PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_mlp PRIVATE m)
add_test(NAME mlp COMMAND test_mlp)

# conv+MLP gradient pipeline test (end-to-end on a separable problem).
add_executable(test_convnet ${CMAKE_SOURCE_DIR}/tests/test_convnet.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c)
target_include_directories(test_convnet PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_convnet PRIVATE m)
add_test(NAME convnet COMMAND test_convnet)
# CPU-heavy full-training smoke test: 60 epochs of conv+MLP. Bump the
# per-test timeout well above the default 10s so a normal `ctest` run
# doesn't flag it as a failure on slow CI boxes.
set_tests_properties(convnet PROPERTIES TIMEOUT 600 LABELS ocr)

# conv3 (3-stage MED) gradient pipeline test (same decisive proof).
add_executable(test_convnet3 ${CMAKE_SOURCE_DIR}/tests/test_convnet3.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c)
target_include_directories(test_convnet3 PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_convnet3 PRIVATE m)
add_test(NAME convnet3 COMMAND test_convnet3)
set_tests_properties(convnet3 PROPERTIES TIMEOUT 1800 LABELS ocr)

# im2col+GEMM conv rewrite: numeric equivalence to brute-force conv + bias-grad FD check.
add_executable(test_convnet3_im2col test_convnet3_im2col.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/dft.c ${CMAKE_SOURCE_DIR}/src/wubuocr/goldplace.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/stylebank.c ${CMAKE_SOURCE_DIR}/src/wubuocr/zoning.c)
target_include_directories(test_convnet3_im2col PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_convnet3_im2col PRIVATE m)
add_test(NAME convnet3_im2col COMMAND test_convnet3_im2col)
set_property(TEST convnet3_im2col PROPERTY LABELS ocr)

# instance-norm (batch-free BatchNorm) gamma/beta gradient finite-diff check.
add_executable(gradcheck_inorm ${CMAKE_SOURCE_DIR}/tests/gradcheck_inorm.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c)
target_include_directories(gradcheck_inorm PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(gradcheck_inorm PRIVATE m)
add_test(NAME inorm_gradcheck COMMAND gradcheck_inorm)

# stylebank: 64MB multi-style expert bank (56 best + 8 rolling) smoke test.
add_executable(test_stylebank ${CMAKE_SOURCE_DIR}/tests/test_stylebank.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/stylebank.c ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c)
target_include_directories(test_stylebank PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_stylebank PRIVATE m)
add_test(NAME stylebank COMMAND test_stylebank)

# emnist_train_conv3 / emnist_infer_conv3: 3-stage MED conv+MLP (2011 build).
add_executable(emnist_infer_conv emnist_infer_conv.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/dft.c ${CMAKE_SOURCE_DIR}/src/wubuocr/goldplace.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/stylebank.c ${CMAKE_SOURCE_DIR}/src/wubuocr/zoning.c)
target_include_directories(emnist_infer_conv PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(emnist_infer_conv PRIVATE m)

# Modular trainer CLI
add_executable(trainer_cli trainer_cli.c ${CMAKE_SOURCE_DIR}/src/wubuocr/trainer.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c)
target_include_directories(trainer_cli PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(trainer_cli PRIVATE m pthread)

# Fast validation trainer (small net, few epochs, capped data)
add_executable(emnist_train_fast emnist_train_fast.c ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c)
target_include_directories(emnist_train_fast PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(emnist_train_fast PRIVATE m pthread)

# emnist_train_conv3 / emnist_infer_conv3: 3-stage MED conv+MLP (2011 build).
add_executable(emnist_train_conv3 emnist_train_conv3.c
               ${CMAKE_SOURCE_DIR}/src/wubuocr/convnet3.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c ${CMAKE_SOURCE_DIR}/src/wubuocr/conv_gemm.c)
target_include_directories(emnist_train_conv3 PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(emnist_train_conv3 PRIVATE m pthread)
# The smoke test needs the downloaded EMNIST IDX corpus under data/emnist/.
# Register it only when that corpus is present, so a `ctest` run on a machine
# without the dataset doesn't report a spurious failure (the binary returns 1
# with no dataset). Add the dataset, then reconfigure to enable the test.
if (EXISTS "${CMAKE_SOURCE_DIR}/data/emnist")
  add_test(NAME emnist_train_conv3_smoke COMMAND emnist_train_conv3 data/emnist 128 64 2 2000)
  set_tests_properties(emnist_train_conv3_smoke PROPERTIES
                        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 1200 LABELS ocr)
endif()

# wubuocr EMNIST Letters training/inference tools (lightweight C11 OCR).
# emnist_train needs the downloaded IDX corpus under data/emnist/ and WuBuMath.
# emnist_infer is dependency-free.
add_executable(emnist_infer emnist_infer.c ${CMAKE_SOURCE_DIR}/src/wubuocr/zoning.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c)
target_include_directories(emnist_infer PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(emnist_infer PRIVATE m)

find_path(WUBUMATH_INCLUDE_DIR NAMES wubumath.h
          PATHS ${CMAKE_SOURCE_DIR}/../WuBuMath/include
                $ENV{HOME}/WuBuMath/include
          NO_DEFAULT_PATH)
find_library(WUBUMATH_SGD  NAMES wubu_riemannian_sgd
             PATHS ${CMAKE_SOURCE_DIR}/../WuBuMath/src/train
                   $ENV{HOME}/WuBuMath/src/train NO_DEFAULT_PATH)
if (WUBUMATH_INCLUDE_DIR AND EXISTS "${WUBUMATH_INCLUDE_DIR}/wubu_riemannian_sgd.h")
  add_executable(emnist_train emnist_train.c ${CMAKE_SOURCE_DIR}/src/wubuocr/zoning.c ${CMAKE_SOURCE_DIR}/src/wubuocr/mlp.c)
  target_include_directories(emnist_train PRIVATE ${WUBUMATH_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/wubuocr)
  target_sources(emnist_train PRIVATE
    ${WUBUMATH_INCLUDE_DIR}/../src/train/wubu_riemannian_sgd.c
    ${WUBUMATH_INCLUDE_DIR}/../src/train/wubu_q_controller.c
    ${WUBUMATH_INCLUDE_DIR}/../src/math/wubu_utils.c
    ${WUBUMATH_INCLUDE_DIR}/../src/math/wubu_hyperbolic.c)
  target_link_libraries(emnist_train PRIVATE m)
endif()
