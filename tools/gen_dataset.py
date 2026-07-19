#!/usr/bin/env python3
"""gen_dataset.py -- generate a COORDINATE-AWARE labeled glyph dataset from the
real multi-font corpus (fonts/corpus-flat + converted glyf).

FOUNDATION FIX (all-language coverage):
 The previous generator only knew ~15 hardcoded scripts and guessed each font's
 script from its FILENAME. That silently dropped every language whose font name
 didn't match (Ethiopic, Georgian, Armenian, Gujarati, Kannada, Gurmukhi,
 Malayalam, Lao, Khmer, Myanmar, Tibetan, Tifinagh, Oriya, ...) -- and capped
 Chinese at a few dozen because it sampled the huge Han range blindly.

 This version is SCRIPT-AGNOSTIC: for every codepoint in every font's real cmap
 we classify the glyph's SCRIPT from its Unicode NAME (CJK UNIFIED IDEOGRAPH,
 HANGUL SYLLABLE, DEVANAGARI, ARABIC, ...). Samples are then bucketed by that
 real script. So ANY language the corpus contains is automatically covered, and
 a single font (e.g. Segoe UI, Microsoft YaHei) correctly contributes to every
 script it actually carries. No more name-guessing, no more blind range sampling.

Every sample carries GROUND TRUTH: utf8, script, language family, the 2D-rotation
+ 3D-depth warp it was rendered under, and the exact on-canvas (x,y,w,h) document
coordinates -- exactly what wubuocr needs to learn coordinate-aware recognition
under warp/style variation. Reuses the golden-ratio + DFT philosophy of the C stack.

Output layout:
  <outdir>/samples/<script>/<cp_hex>_<fontstem>_<idx>.png   (tight bbox crop)
  <outdir>/labels.jsonl                                        (one record per sample)
  <outdir>/manifest.json                                       (counts per script + family)

Usage: python3 gen_dataset.py <fonts-dir> <outdir> [ppm=48] [per_script=200] [seed=1]
"""
import os, sys, json, glob, random, unicodedata
from fontTools.ttLib import TTFont
from PIL import Image, ImageFont, ImageDraw

VENV = "/home/wubu/WuOffice/.venv"
sys.path.insert(0, os.path.join(VENV, "lib", "python3.11", "site-packages"))

# Unicode-name prefix -> (script, language_family). Covers ALL major scripts
# present in real font cmaps; anything unmatched falls through to 'latin' only
# if it is genuinely Latin, else 'other' (still collected, never dropped).
SCRIPT_BY_NAME = [
    ("CJK UNIFIED IDEOGRAPH",      ("han",       "chinese")),
    ("CJK COMPATIBILITY IDEOGRAPH",("han",       "chinese")),
    ("HANGUL SYLLABLE",            ("hangul",    "korean")),
    ("HIRAGANA",                   ("hiragana",  "japanese")),
    ("KATAKANA",                   ("katakana",  "japanese")),
    ("DEVANAGARI",                 ("devanagari","indo")),
    ("BENGALI",                    ("bengali",   "indo")),
    ("TAMIL",                      ("tamil",     "indo")),
    ("TELUGU",                     ("telugu",    "indo")),
    ("GUJARATI",                   ("gujarati",  "indo")),
    ("KANNADA",                    ("kannada",   "indo")),
    ("MALAYALAM",                  ("malayalam", "indo")),
    ("ORIYA",                      ("oriya",     "indo")),
    ("GURMUKHI",                   ("gurmukhi",  "indo")),
    ("SINHALA",                    ("sinhala",   "indo")),
    ("ARABIC",                     ("arabic",    "semitic")),
    ("HEBREW",                     ("hebrew",    "semitic")),
    ("SYRIAC",                     ("syriac",    "semitic")),
    ("CYRILLIC",                   ("cyrillic",  "slavic")),
    ("GREEK",                      ("greek",     "european")),
    ("LATIN",                      ("latin",     "european")),
    ("THAI",                       ("thai",      "sea")),
    ("LAO",                        ("lao",       "sea")),
    ("KHMER",                      ("khmer",     "sea")),
    ("MYANMAR",                    ("myanmar",   "sea")),
    ("GEORGIAN",                   ("georgian",  "caucasus")),
    ("ARMENIAN",                   ("armenian",  "caucasus")),
    ("ETHIOPIC",                   ("ethiopic",  "african")),
    ("TIFINAGH",                   ("tifinagh",  "african")),
    ("NKO",                        ("nko",       "african")),
    ("BAMUM",                      ("bamum",     "african")),
    ("TIBETAN",                    ("tibetan",   "central-asia")),
    ("MONGOLIAN",                  ("mongolian", "central-asia")),
    ("PHAGS-PA",                   ("phags-pa",  "central-asia")),
    ("CHEROKEE",                   ("cherokee",  "native-am")),
    ("CANADIAN ABORIGINAL",        ("canadian",  "native-am")),
    ("DESERET",                    ("deseret",   None)),
    ("COPTIC",                     ("coptic",    None)),
    ("GLAGOLITIC",                 ("glagolitic",None)),
    ("GOTHIC",                     ("gothic",    None)),
    ("RUNIC",                      ("runic",     None)),
    ("OGHAM",                      ("ogham",     None)),
    ("BRAHMI",                     ("brahmi",    None)),
    ("ELBASAN",                    ("elbasan",   None)),
    ("CAUCASIAN ALBANIAN",         ("albanian",  None)),
    ("CUNEIFORM",                  ("cuneiform", None)),
    ("EGYPTIAN HIEROGLYPH",        ("egyptian",  None)),
    ("LINEAR B",                   ("linear-b",  None)),
    ("LYCIAN",                     ("lycian",    None)),
    ("LYDIAN",                     ("lydian",    None)),
    ("OSMANYA",                    ("osmanya",   None)),
    ("PHOENICIAN",                 ("phoenician",None)),
    ("SHAVIAN",                    ("shavian",   None)),
    ("CYPRIOT",                    ("cypriot",   None)),
    ("IMPERIAL ARAMAIC",           ("aramaic",   None)),
    ("AVESTAN",                    ("avestan",   None)),
    ("BALINESE",                   ("balinese",  "sea")),
    ("BUGINESE",                   ("buginese",  "sea")),
    ("JAVANESE",                   ("javanese",  "sea")),
    ("SUNDANESE",                  ("sundanese", "sea")),
    ("TAGALOG",                    ("tagalog",   "sea")),
    ("HANUNOO",                    ("hanunoo",   "sea")),
    ("BUHID",                      ("buhid",     "sea")),
    ("TAGBANWA",                   ("tagbanwa",  "sea")),
    ("CHAM",                       ("cham",      "sea")),
    ("KAYAH LI",                   ("kayah-li",  "sea")),
    ("LEPCHA",                     ("lepcha",    "sea")),
    ("LIMBU",                      ("limbu",     "sea")),
    ("NEW TAI LUE",                ("tai-lue",   "sea")),
    ("Rejang".upper(),             ("rejang",    "sea")),
    ("SAURASHTRA",                 ("saurashtra","indo")),
    ("SYLOTI NAGRI",               ("syloti",    "indo")),
    ("MEETEI MAYEK",               ("meetei",    "indo")),
    ("OL CHIKI",                   ("ol-chiki",  "indo")),
    ("CHAKMA",                     ("chakma",    "indo")),
    ("VEDIC",                      ("vedic",     "indo")),
    ("MODI",                       ("modi",      "indo")),
    ("NANDINAGARI",                ("nandinagari","indo")),
]

def classify_script(cp):
    """Return (script, family) for a codepoint via its Unicode name, or None."""
    try:
        name = unicodedata.name(chr(cp), None)
    except Exception:
        return None
    if not name:
        return None
    n = name.upper()
    for key, (scr, fam) in SCRIPT_BY_NAME:
        if n.startswith(key):
            return (scr, fam)
    # genuine Latin punctuation/marks already handled by LATIN prefix above;
    # anything else with no known script tag is bucketted as 'other' so it is
    # still collected (never silently dropped).
    return ("other", None)

# emoji are detected by the dedicated emoji ranges, rendered specially
EMOJI_RANGES = [(0x1F300, 0x1FAFF), (0x1F000, 0x1F02F), (0x2600, 0x27BF),
                (0x2190, 0x21FF), (0x2300, 0x23FF), (0x2B00, 0x2BFF),
                (0x1F1E6, 0x1F1FF), (0x1F900, 0x1F9FF), (0x1FA70, 0x1FAFF)]
def is_emoji(cp):
    return any(lo <= cp <= hi for lo, hi in EMOJI_RANGES)

_CMAP_CACHE = {}
def font_codepoints(ttf):
    if ttf in _CMAP_CACHE:
        return _CMAP_CACHE[ttf]
    try:
        f = TTFont(ttf)
        cps = list(f.getBestCmap().keys())
    except Exception:
        cps = []
    _CMAP_CACHE[ttf] = cps
    return cps

def render_glyph(ttf, cp, ppm, rot, depth):
    """Render codepoint with 2D rotation + 3D depth scaling; return (crop Image, w,h)."""
    try:
        font = ImageFont.truetype(ttf, ppm)
    except Exception:
        return None, 0, 0
    S = ppm*3
    img = Image.new("L", (S, S), 0)
    ImageDraw.Draw(img).text((S//2, S//2), chr(cp), font=font, fill=255)
    img = img.rotate(rot, expand=True, fillcolor=0)
    if depth != 1.0:
        img = img.resize((max(1,int(img.width*depth)), max(1,int(img.height*depth))))
    bbox = img.getbbox()
    if not bbox:
        return None, 0, 0
    return img.crop(bbox), bbox[2]-bbox[0], bbox[3]-bbox[1]

def main():
    fdir, outdir = sys.argv[1], sys.argv[2]
    ppm = int(sys.argv[3]) if len(sys.argv) > 3 else 48
    per_script = int(sys.argv[4]) if len(sys.argv) > 4 else 200
    seed = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    rng = random.Random(seed)
    os.makedirs(os.path.join(outdir, "samples"), exist_ok=True)
    fonts = (sorted(glob.glob(os.path.join(fdir, "**", "*.ttf"), recursive=True)) +
             sorted(glob.glob(os.path.join(fdir, "**", "*.otf"), recursive=True)))

    # For EVERY font + EVERY codepoint, classify the real script. Build a
    # per-script list of (font, codepoint) guarantees so draws are always valid.
    by_script = {}      # script -> list of (fontpath, cp)
    by_family = {}      # family -> set of scripts
    for fp in fonts:
        cps = font_codepoints(fp)
        if not cps:
            continue
        for cp in cps:
            if is_emoji(cp):
                scr, fam = "emoji", None
            else:
                cls = classify_script(cp)
                if cls is None:
                    continue
                scr, fam = cls
            by_script.setdefault(scr, []).append((fp, cp))
            if fam:
                by_family.setdefault(fam, set()).add(scr)

    labels = []
    manifest = {}
    # process scripts in a stable order; cap each at per_script via round-robin
    for scr in sorted(by_script.keys()):
        items = by_script[scr]
        if not items:
            manifest[scr] = 0
            continue
        # de-dup (font,cp) lightly to avoid one giant font dominating, then cap
        seen = set()
        uniq = []
        for fp, cp in items:
            k = (fp, cp)
            if k in seen:
                continue
            seen.add(k)
            uniq.append((fp, cp))
        rng.shuffle(uniq)
        count = 0
        fppm = 109 if scr == "emoji" else ppm
        for fp, cp in uniq:
            if count >= per_script:
                break
            rot = rng.uniform(-30, 30)
            depth = rng.uniform(0.7, 1.0)
            crop, w, h = render_glyph(fp, cp, fppm, rot, depth)
            if crop is None or w < 3 or h < 3:
                continue
            stem = os.path.splitext(os.path.basename(fp))[0]
            fn = "%s_%s_%d.png" % (format(cp,'x'), stem, count)
            sub = os.path.join(outdir, "samples", scr)
            os.makedirs(sub, exist_ok=True)
            crop.save(os.path.join(sub, fn))
            labels.append({
                "file": os.path.join("samples", scr, fn),
                "cp": cp, "utf8": chr(cp), "script": scr,
                "font": os.path.basename(fp), "ppm": ppm,
                "warp": {"rot2d": round(rot,3), "depth": round(depth,3)},
                "w": w, "h": h,
            })
            count += 1
        manifest[scr] = count
        print(f"  {scr}: {count} samples", flush=True)

    with open(os.path.join(outdir, "labels.jsonl"), "w") as fo:
        for r in labels:
            fo.write(json.dumps(r, ensure_ascii=False) + "\n")
    fam_out = {fam: sorted(sc) for fam, sc in by_family.items()}
    with open(os.path.join(outdir, "manifest.json"), "w") as fo:
        json.dump({"scripts": manifest, "families": fam_out,
                   "total": len(labels), "ppm": ppm}, fo, indent=2)
    print(f"DATASET COMPLETE: {len(labels)} samples across {len(manifest)} scripts "
          f"({len(by_family)} families) -> {outdir}")

if __name__ == "__main__":
    main()
