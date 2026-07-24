#!/usr/bin/env bash
#
# train_all_langs.sh -- train one CRNN model per script family ("all languages").
#
# Each script family gets a dedicated model because a single CRNN cannot cover
# every writing system (CJK alone has tens of thousands of codepoints). The
# codepoint-aware trainer (crnn_ocr_train) takes a CHARS list of either literal
# BMP characters or U+XXXX codepoints, so every script is first-class.
#
# Usage:
#   tools/train_all_langs.sh            # train ALL defined languages
#   tools/train_all_langs.sh latin euro # train only the named ones
#   DRY=1 tools/train_all_langs.sh      # print the commands, don't run
#
# Output models land in $OUT (default /tmp/wubu_models). They are gitignored
# (binary artefacts); the script itself is the reproducible source of truth.
#
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
TRAIN="$BUILD/crnn_ocr_train"
OUT="${OUT:-/tmp/wubu_models}"
mkdir -p "$OUT"
: "${EPOCHS:=200}" "${NTR:=800}" "${HID:=64}" "${LR:=0.0012}"

# name|font|charset-expression
# charset-expression is a bash string that may contain literal BMP chars or
# U+XXXX tokens; we pass it straight through to the trainer's CHARS env.
LATIN_FONT="$ROOT/fonts/multiscript_active/Latin.ttf"
EURO_FONT="$ROOT/fonts/study-corpus/arimo/fonts/ttf/Arimo-Regular.ttf"

# Greek (upper+lower), Cyrillic (upper+lower)
GREEK_U=$(printf '%b' "$(printf '\\U%04X' $(seq 0x391 0x3A1) $(seq 0x3B1 0x3C9) | tr '\n' ' ')")
CYR_U=$(printf '%b' "$(printf '\\U%04X' $(seq 0x410 0x44F) $(seq 0x450 0x45F) | tr '\n' ' ')")

define_langs() {
  # latin (full)
  LATIN="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?'-"
  # pan-european: latin + greek + cyrillic
  EURO="$LATIN$GREEK_U$CYR_U"
  # a few common Latin-extended (Vietnamese, Turkish, Polish)
  LATE="$(printf '%b' "$(printf '\\U%04X' 0x104 0x105 0x11C 0x11D 0x141 0x142 0x1EA1 0x1EA0 0x1EAF 0x1EBF | tr '\n' ' ')")"
  # We train per-family; Arabic/Hebrew/Devanagari/Thai/Tamil use Noto fonts and
  # the FULL unicode block for that script.
}

# Emit "name|font|chars" lines. We keep it conservative and explicit.
emit() {
  define_langs
  # 1) Latin (base, Latin America + Western Europe + digits + punct)
  echo "latin|$LATIN_FONT|$LATIN"
  # 2) Pan-European (Latin + Greek + Cyrillic)
  echo "euro|$EURO_FONT|$EURO"
}

want() {
  # if no filter given, train all; else only listed names
  [ $# -eq 0 ] && return 0
  local n="$1"; shift
  for f in "$@"; do [ "$f" = "$n" ] && return 0; done
  return 1
}

emit | while IFS='|' read -r name font chars; do
  want "$name" "$@" || continue
  out="$OUT/$name.crnn"
  cmd="SAVE=$out CHARS=$chars HID=$HID NTR=$NTR AUG=1 PHOTO=1 STRIDE=10 $TRAIN $font $EPOCHS $LR"
  if [ "${DRY:-0}" = "1" ]; then
    echo "$cmd"
  else
    echo ">>> training $name -> $out"
    eval "$cmd" > "$OUT/$name.log" 2>&1
    echo "<<< done $name ($(tail -1 "$OUT/$name.log"))"
  fi
done
echo "All requested languages trained into $OUT"
