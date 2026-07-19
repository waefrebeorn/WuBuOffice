#!/usr/bin/env bash
# build_wubu_ocr.sh -- compile the wubu-trained EMNIST OCR tools against WuBuMath.
# Pure C11, no external deps beyond the bundled WuBuMath + OCR modules.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
WUBUMATH="$HERE/../WuBuMath"
# _POSIX_C_SOURCE exposes strdup/open_memstream under strict -std=c11 so the
# compiler does NOT assume an `int` return and truncate 64-bit char* pointers
# (a latent segfault). The CMake build sets this globally; keep the script in
# sync.
INC="-D_POSIX_C_SOURCE=200809L -I$WUBUMATH/include -I$HERE/src/wubuocr -I$HERE/src/wubufont -I$HERE/src/wubujson -I$HERE/src/wubuzip"
WM_SRC="$WUBUMATH/src/train/wubu_riemannian_sgd.c \
       $WUBUMATH/src/train/wubu_q_controller.c \
       $WUBUMATH/src/math/wubu_utils.c \
       $WUBUMATH/src/math/wubu_hyperbolic.c"
OCR_SRC="$HERE/src/wubuocr/zoning.c $HERE/src/wubuocr/mlp.c $HERE/src/wubuocr/convnet.c"
# -Wno-misleading-indentation / -Wno-unused-result: stylistic only for these
# compact single-pass tools; correctness is covered by -Wall -Wextra -Wpedantic.
WARN="-Wall -Wextra -Wpedantic -Wno-misleading-indentation -Wno-unused-result"
mkdir -p build
cc -std=c11 -O2 $WARN $INC tools/emnist_train.c $OCR_SRC $WM_SRC \
    -o build/emnist_train -lm
echo "built build/emnist_train"
# Inference tools are dependency-free (no WuBu math needed at inference time).
cc -std=c11 -O2 $WARN $INC tools/emnist_infer.c $OCR_SRC \
    -o build/emnist_infer -lm
echo "built build/emnist_infer"
# Ultra-light conv+MLP trainer (dependency-free plain C11 SGD).
cc -std=c11 -O2 $WARN $INC tools/emnist_train_conv.c $OCR_SRC \
    -o build/emnist_train_conv -lm
echo "built build/emnist_train_conv"
cc -std=c11 -O2 $WARN $INC tools/emnist_infer_conv.c $OCR_SRC \
    -o build/emnist_infer_conv -lm
echo "built build/emnist_infer_conv"
# Ultra-light 3-stage conv+MLP trainer (dependency-free plain C11, multithreaded).
cc -std=c11 -O2 -pthread $WARN $INC tools/emnist_train_conv3.c \
    $HERE/src/wubuocr/convnet3.c $HERE/src/wubuocr/mlp.c \
    -o build/emnist_train_conv3 -lm
echo "built build/emnist_train_conv3"
# Dependency-free unit test for the conv3 (3-stage) gradient pipeline.
cc -std=c11 -O2 $WARN $INC tests/test_convnet3.c \
    $HERE/src/wubuocr/convnet3.c $HERE/src/wubuocr/mlp.c \
    -o build/test_convnet3 -lm
echo "built build/test_convnet3"

# 64MB multi-style expert bank (56 best + 8 rolling) smoke test.
cc -std=c11 -O2 $WARN $INC tests/test_stylebank.c \
    $HERE/src/wubuocr/stylebank.c $HERE/src/wubuocr/convnet3.c $HERE/src/wubuocr/mlp.c \
    -o build/test_stylebank -lm
echo "built build/test_stylebank"

# End-to-end coordinate-aware OCR ingestion demo (wires compose -> analyze ->
# JSON, with DFT compression + golden-ratio warp). Links the full wubuocr
# stack + wubufont + wubujson + wubuzip (woff needs inflate).
cc -std=c11 -O2 $WARN $INC tools/ocringest.c \
    $HERE/src/wubuocr/*.c $HERE/src/wubufont/*.c \
    $HERE/src/wubujson/*.c $HERE/src/wubuzip/*.c \
    -o build/ocringest -lm
echo "built build/ocringest"
