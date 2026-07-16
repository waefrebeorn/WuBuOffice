#!/usr/bin/env bash
# build_wubu_ocr.sh -- compile the wubu-trained EMNIST OCR tool against WuBuMath.
# Pure C11, no external deps. Run from the WuBuOffice repo root.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
WUBUMATH="$HERE/../WuBuMath"
SRC="$HERE/tools/emnist_train_wubu.c"
WM_SRC="$WUBUMATH/src/train/wubu_riemannian_sgd.c \
       $WUBUMATH/src/train/wubu_q_controller.c \
       $WUBUMATH/src/math/wubu_utils.c \
       $WUBUMATH/src/math/wubu_hyperbolic.c"
INC="-I$WUBUMATH/include"
mkdir -p build
cc -std=c11 -O2 -Wall -Wextra -Wpedantic $INC $SRC $WM_SRC \
    -o build/emnist_train_wubu -lm
echo "built build/emnist_train_wubu"
