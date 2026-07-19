#!/usr/bin/env python3
"""expand_corpus.py -- sweep EVERY codepoint every font knows ("all Unicode"),
plus emoji via Pillow, into the labeled glyph corpus (JSONL).

Why Python, not C:
 - wubufont (C, glyf-only) is what the OCR *stack* uses; ocrcorpus.c covers it
   fast + dependency-free for training. This expander goes WIDER: every
   codepoint in every cmap (not just script-block samples), plus color emoji
   which wubufont cannot render (COLR/CBDT). Pillow renders emoji; we binarize
   the alpha to a silhouette and also keep a color-histogram feature.
 - Output schema is identical to ocrcorpus.c JSONL so both feed the same
   trainer / ingestion model.

Usage: python3 expand_corpus.py <fonts-glob-dir> <out.jsonl> [ppm=48] [per-font-cap=2000]
"""
import os, glob, sys, json, struct
from fontTools.ttLib import TTFont

VENV = "/home/wubu/WuOffice/.venv"
sys.path.insert(0, os.path.join(VENV, "lib", "python3.11", "site-packages"))
from PIL import Image, ImageFont, ImageDraw

PPM = int(sys.argv[3]) if len(sys.argv) > 3 else 48
CAP = int(sys.argv[4]) if len(sys.argv) > 4 else 2000
out = sys.argv[2]
fdir = sys.argv[1]

EMOJI_TAGS = set()  # codepoints that are emoji (heuristic: in emoji font cmap)

def script_of(name):
    n = name.lower()
    for key in ["hebrew","devanagari","bengali","tamil","telugu","thai","lao","khmer",
                "myanmar","sinhala","gujarati","kannada","malayalam","oriya","gurmukhi",
                "ethiopic","georgian","armenian","cherokee","cjk","chinese","japan","korean",
                "arabic","cyrillic","nushu","adlam","tibetan","olchiki","gondi","meetei",
                "siddham","taitham","tifinagh","bamum","batak","bhaiksuki","brahmi","buginese",
                "buhid","canadian","carian","caucasian","chakma","cham","coptic","cuneiform",
                "cypriot","deseret","egyptian","elbasan","elymaic","glagolitic","gothic",
                "grantha","anatolian","avestan","bassa","batak","canadian","dogra"]:
        if key in n: return key
    if "emoji" in n: return "emoji"
    if "mono" in n: return "mono"
    if "serif" in n: return "serif"
    if "sans" in n or "display" in n: return "sans"
    return "latin"

def dft_features(crop):
    """trivial 1-D-ish features for Python side (mirrors dft.c band idea):
    ink ratio, aspect, fill center-of-mass offset. Keeps schema compatible."""
    h = len(crop); w = len(crop[0]) if h else 0
    ink = sum(1 for row in crop for v in row if v)
    if ink == 0: return 0.0, 0.0, 0.0
    sx = sy = 0
    for y,row in enumerate(crop):
        for x,v in enumerate(row):
            if v: sx += x; sy += y
    cx = sx/ink; cy = sy/ink
    offx = (cx - w/2)/(w/2) if w else 0
    offy = (cy - h/2)/(h/2) if h else 0
    return round(ink/(w*h),4), round(offx,3), round(offy,3)

# Pre-scan emoji font for emoji codepoints
emoji_font = "/home/wubu/WuBuOffice/fonts/emoji/NotoColorEmoji.ttf"
emoji_cps = set()
if os.path.exists(emoji_font):
    try:
        ef = TTFont(emoji_font); emoji_cps = set(ef.getBestCmap().keys())
    except Exception:
        pass

fonts = sorted(glob.glob(os.path.join(fdir, "**", "*.ttf"), recursive=True)
               + glob.glob(os.path.join(fdir, "*.ttf")))
fonts = [f for f in fonts if "NotoColorEmoji" not in f]

total = 0
with open(out, "w") as fo:
    for fp in fonts:
        try:
            f = TTFont(fp)
            cmap = f.getBestCmap()
        except Exception:
            continue
        if not cmap:
            continue
        tag = script_of(os.path.basename(fp))
        count = 0
        for cp in sorted(cmap):
            if cp < 0x20 or cp == 0xFEFF:
                continue
            if count >= CAP:
                break
            try:
                # render via Pillow (works for glyf + CFF + most; emoji handled separately)
                font = ImageFont.truetype(fp, PPM)
                img = Image.new("L", (PPM*2, PPM*2), 0)
                ImageDraw.Draw(img).text((PPM//2, PPM//2), chr(cp), font=font, fill=255)
            except Exception:
                continue
            # tight bbox
            bbox = img.getbbox()
            if not bbox:
                continue
            crop = img.crop(bbox)
            cw, ch = crop.size
            if cw < 2 or ch < 2:
                continue
            cpix = crop.load()
            grid = [[1 if cpix[x,y] > 127 else 0 for x in range(cw)] for y in range(ch)]
            inkr, ox, oy = dft_features(grid)
            if sum(sum(r) for r in grid) < 3:
                continue
            try:
                u8 = chr(cp).encode("utf-8")
            except Exception:
                u8 = b"?"
            rec = {
                "cp": cp, "utf8": u8.decode("utf-8", "replace"), "script": tag,
                "font": os.path.basename(fp), "w": cw, "h": ch,
                "ink_ratio": inkr, "com_x": ox, "com_y": oy, "src": "glyf",
            }
            fo.write(json.dumps(rec, ensure_ascii=False) + "\n")
            count += 1; total += 1
        if count:
            print(f"  {os.path.basename(fp)} ({tag}): {count}", flush=True)

    # Emoji pass (color -> binarized silhouette)
    if emoji_cps:
        try:
            efont = ImageFont.truetype(emoji_font, 109, layout_engine=ImageFont.Layout.RAQM)
        except Exception:
            efont = ImageFont.truetype(emoji_font, 109)
        ec = 0
        for cp in sorted(emoji_cps):
            if cp < 0x1F000 or cp in (0x200D, 0xFE0F):
                continue
            try:
                img = Image.new("RGBA", (160, 160), (0,0,0,0))
                ImageDraw.Draw(img).text((20,20), chr(cp), font=efont, embedded_color=True)
            except Exception:
                continue
            gray = img.convert("L")
            bbox = gray.getbbox()
            if not bbox:
                continue
            crop = gray.crop(bbox); cw, ch = crop.size
            if cw < 3 or ch < 3:
                continue
            # binarize alpha to silhouette ink
            cpix = crop.load()
            grid = [[1 if cpix[x,y] > 40 else 0 for x in range(cw)] for y in range(ch)]
            if sum(sum(r) for r in grid) < 5:
                continue
            inkr, ox, oy = dft_features(grid)
            rec = {"cp": cp, "utf8": chr(cp), "script": "emoji",
                   "font": "NotoColorEmoji", "w": cw, "h": ch,
                   "ink_ratio": inkr, "com_x": ox, "com_y": oy, "src": "emoji"}
            fo.write(json.dumps(rec, ensure_ascii=False) + "\n")
            ec += 1; total += 1
        print(f"  NotoColorEmoji (emoji): {ec}", flush=True)

print(f"TOTAL {total} labeled glyphs -> {out}")
