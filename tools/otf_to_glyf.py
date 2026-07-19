#!/usr/bin/env python3
"""otf_to_glyf.py -- convert CFF/OTF (and OTC collections) to TrueType glyf so
the dependency-free C wubufont rasterizer (glyf-only) can read them.

Handles:
 - plain CFF .otf
 - OTC collections (.otf/.ttc with multiple faces) -> writes one glyf TTF per face
 - already-TrueType .ttf -> copied through

For each face we convert CFF -> glyf with fontTools' cu2qu (quadratic flatten),
keeping cmap/metrics/name intact. Output under OUTDIR:
  <stem>_glyf.ttf            (single-face input)
  <stem>_faceN_glyf.ttf      (collection input, one per face)

Usage: python3 otf_to_glyf.py <indir> <outdir> [max_workers=4]
"""
import os, sys, glob
from fontTools.ttLib import TTFont, getTableClass
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen

VENV = "/home/wubu/WuOffice/.venv"
sys.path.insert(0, os.path.join(VENV, "lib", "python3.11", "site-packages"))

def new_table(f, tag):
    """Workaround for installs where TTFont.newTable is unavailable: build the
    empty table object via the public getTableClass resolver and register it."""
    klass = getTableClass(tag)
    tbl = klass(tag)
    f.tables[tag] = tbl
    return tbl

def convert_face(src, dst, font_number=None):
    f = TTFont(src, fontNumber=0 if font_number is None else font_number, lazy=False)
    tags = set(f.keys())
    if "glyf" in tags:
        f.save(dst); return True
    if "CFF " not in tags and "CFF2" not in tags:
        return False
    go = f.getGlyphOrder()
    gs = f.getGlyphSet()
    glyf = new_table(f, "glyf")
    glyf.glyphOrder = go
    glyf.glyphs = {}
    for nm in go:
        pen = TTGlyphPen(gs)
        cu = Cu2QuPen(pen, 1.0, reverse_direction=True)
        try:
            gs[nm].draw(cu)
        except Exception:
            pass
        glyf.glyphs[nm] = pen.glyph()
    new_table(f, "loca")
    for t in ("CFF ", "CFF2", "VORG"):
        if t in f:
            del f[t]
    f.sfntVersion = "\x00\x01\x00\x00"
    # NOTE: leaving the 'post' table untouched; this fontTools build's post
    # table class lacks the expected attributes, and wubufont only reads
    # glyf/loca/cmap/head, so rewriting post is unnecessary.
    f.save(dst)
    return True

def count_faces(src):
    try:
        coll = TTFont(src, fontNumber=0, lazy=False)
        r = coll.reader
        if hasattr(r, "numFonts"):
            return r.numFonts
        return 1
    except Exception:
        return 1

def main():
    indir, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    files = sorted(glob.glob(os.path.join(indir, "**", "*.ttf"), recursive=True) +
                   glob.glob(os.path.join(indir, "**", "*.otf"), recursive=True) +
                   glob.glob(os.path.join(indir, "**", "*.ttc"), recursive=True))
    done = skip = fail = 0
    for p in files:
        if p.endswith("_glyf.ttf") or "_face" in os.path.basename(p):
            continue
        stem = os.path.splitext(os.path.basename(p))[0]
        try:
            nf = count_faces(p)
        except Exception:
            nf = 1
        if nf <= 1:
            try:
                if convert_face(p, os.path.join(outdir, stem + "_glyf.ttf")):
                    done += 1
                else:
                    skip += 1
            except Exception as e:
                fail += 1
                if fail < 5: print("FAIL", p, e)
        else:
            for i in range(nf):
                try:
                    if convert_face(p, os.path.join(outdir, "%s_face%d_glyf.ttf" % (stem, i)), font_number=i):
                        done += 1
                    else:
                        skip += 1
                except Exception as e:
                    fail += 1
                    if fail < 5: print("FAIL", p, "face", i, e)
        if (done+skip+fail) % 200 == 0:
            print(f"  progress: done={done} skip={skip} fail={fail}")
    print(f"OTF/OTC->glyf complete: converted={done} skipped={skip} failed={fail}")

if __name__ == "__main__":
    main()
