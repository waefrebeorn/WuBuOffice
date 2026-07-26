#!/bin/bash
# Train CRNN (GRU, CUDA-accelerated) on all 14 multiscript fonts to ~96% char-acc.
# Usage: train_all_gru.sh [epochs] [hid]
set -e
cd /home/wubu/WuBuOffice
export LD_LIBRARY_PATH=/usr/lib/wsl/lib:/usr/local/cuda/targets/x86_64-linux/lib
TRAIN=./build_cuda/crnn_ocr_train
EPOCHS=${1:-80}
HID=${2:-48}
GRAD_CLIP=5.0
NTR=400
LR=0.0012
OUT=/tmp/gru_models
mkdir -p "$OUT"
FONTS=(Arabic Bengali ChineseSC ChineseTC Cyrillic Devanagari Hispanic Japanese Korean Latin Noto_Sans_Telugu Tamil Telugu Thai)
for f in "${FONTS[@]}"; do
  font="fonts/multiscript_active/$f.ttf"
  [ -f "$font" ] || { echo "SKIP missing $font"; continue; }
  out="$OUT/${f}.crnn"
  echo "===== TRAIN $f (HID=$HID EPOCHS=$EPOCHS) ====="
  GRAD_CLIP=$GRAD_CLIP NTR=$NTR HID=$HID RNN_TYPE=2 SAVE="$out" \
    $TRAIN "$font" $EPOCHS $LR 2>&1 | grep -E "epoch|DECODE|final" | tail -8
  echo "saved $out ($(stat -c%s "$out" 2>/dev/null) bytes)"
done
echo "ALL DONE"
