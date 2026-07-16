#!/usr/bin/env bash
# fetch_font_families.sh -- assemble a large open-font study corpus by
# cloning many INDIVIDUAL OFL font-family repos (fast, shallow) instead of
# the monolithic google/fonts tree (which is too large to pull quickly).
#
# Each family repo is tiny + shallow; cloning a few hundred in parallel gives
# the WuBuOCR bank huge glyph-shape variety to study via:
#     wubugauntlet_cli --fontdir fonts/study-corpus --compose
#
# Usage: tools/fetch_font_families.sh [TARGET_DIR] [MAX_PARALLEL]
set -u
TARGET="${1:-fonts/study-corpus}"
MAXP="${2:-8}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/$TARGET"
mkdir -p "$DEST"

# A curated list of well-known OFL/Apache open-font families hosted on GitHub.
# (googlefonts/* are the canonical upstreams; a few are independent.)
FAMILIES=(
  googlefonts/roboto
  googlefonts/roboto-flex
  googlefonts/open-sans
  googlefonts/inter
  googlefonts/noto-sans
  googlefonts/source-sans
  googlefonts/source-serif
  googlefonts/source-code-pro
  googlefonts/lora
  googlefonts/merriweather
  googlefonts/ptsans
  googlefonts/pt-serif
  googlefonts/pt-mono
  googlefonts/fira
  googlefonts/fira-code
  googlefonts/jetbrains-mono
  googlefonts/inconsolata
  googlefonts/ubuntu
  googlefonts/cantarell
  googlefonts/crimson-text
  googlefonts/playfair-display
  googlefonts/montserrat
  googlefonts/poppins
  googlefonts/nunito
  googlefonts/work-sans
  googlefonts/rubik
  googlefonts/comfortaa
  googlefonts/raleway
  googlefonts/oxygen
  googlefonts/dosis
  googlefonts/questrial
  googlefonts/arimo
  googlefonts/cousine
  googlefonts/tono
  googlefonts/creteround
  googlefonts/josefin-sans
  googlefonts/bitter
  googlefonts/cardo
  googlefonts/eb-garamond
  googlefonts/librefranklin
  googlefonts/heebo
  googlefonts/manrope
  googlefonts/mulish
  googlefonts/tilt
  googlefonts/baloo2
  googlefonts/hind
  googlefonts/gothic-a1
  googlefonts/noto-cjk
  theleagueofmoveabletype/fanwood
  theleagueofmoveabletype/league-gothic
  theleagueofmoveabletype/league-spartan
  theleagueofmoveabletype/junction
  theleagueofmoveabletype/ostrich-sans
  theleagueofmoveabletype/propaganda
  bBoxType/Vollkorn
  impallari/lobster
  impallari/cabin
  impallari/dosis
  kkos/onest
  etcetera-io/spline-sans
  fortawesome/font-awesome
)

clone_one() {
  local repo="$1"
  local name="${repo##*/}"
  [ -d "$DEST/$name" ] && return 0
  # Per-repo timeout: a single huge family must not block the whole queue.
  timeout 600 git clone --depth 1 --filter=blob:none "https://github.com/$repo.git" "$DEST/$name" >/dev/null 2>&1 \
    && echo "  + $name" || { echo "  - skip $repo (clone failed/timeout)"; rm -rf "$DEST/$name"; }
}
export -f clone_one
export DEST

printf '%s\n' "${FAMILIES[@]}" | xargs -P "$MAXP" -I{} bash -c 'clone_one "$@"' _ {}

N=$(find "$DEST" -type f \( -iname '*.ttf' -o -iname '*.otf' -o -iname '*.ttc' \) | wc -l)
echo "== Study corpus ready: $N font files across $(ls -1 "$DEST" | wc -l) families =="
echo "== Study them with: wubugauntlet_cli --fontdir \"$DEST\" --compose =="
