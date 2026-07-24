#!/usr/bin/env python3
"""Reproducible OCR accuracy harness for the WuBuOffice pipeline.

Generates deterministic synthetic pages (clean + photo-real) across a set of
fixed seeds, runs `wubuoffice ocr` with a given model, and reports mean
character accuracy + line-segmentation quality (blocks vs GT lines).

Usage:
  acc_harness.py <model.crnn> <font.ttf> [seeds...]
  (reads CHARS from env, default A..Z + digits + punct)
"""
import json, re, subprocess, sys, os, tempfile, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
OCR = os.path.join(BUILD, "wubuoffice")
PGMP = os.path.join(BUILD, "gen_pgmpage")
REALP = os.path.join(BUILD, "gen_realpage")

CHARS = os.environ.get("CHARS",
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'-")

def run(cmd, env=None):
    e = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
    if env:
        e.update(env)
    return subprocess.run(cmd, capture_output=True, text=True, env=e)

def score(gt_lines, pred_blocks):
    tot = cor = 0
    for g, p in zip(gt_lines, pred_blocks):
        n = max(len(g), len(p))
        for i in range(n):
            tot += 1
            if i < len(g) and i < len(p) and g[i] == p[i]:
                cor += 1
    return cor, tot

def eval_clean(model, font, seed):
    d = tempfile.mkdtemp()
    pgm = os.path.join(d, "p.pgm"); gt = os.path.join(d, "gt.txt")
    out = os.path.join(d, "o.json")
    r = run([PGMP, font, pgm], env={"SEED": str(seed), "CHARS": CHARS})
    with open(gt, "w") as f:
        f.write(r.stdout)
    r = run([OCR, "ocr", pgm, out], env={**os.environ, "LOAD": model,
                                         "CHARS": CHARS, "ASAN_OPTIONS": "detect_leaks=0"})
    gts = []
    for line in open(gt):
        m = re.match(r"GT line \d+: (.+)", line)
        if m: gts.append(m.group(1))
    try:
        ps = [b.get("text", "").strip() for b in json.load(open(out))["blocks"]]
    except Exception:
        ps = []
    c, t = score(gts, ps)
    shutil.rmtree(d, ignore_errors=True)
    return c, t, len(gts), len(ps)

def eval_real(model, font):
    # gen_realpage is internally deterministic (fixed rndf seed)
    d = tempfile.mkdtemp()
    png = os.path.join(d, "p.png"); gt = os.path.join(d, "gt.txt")
    out = os.path.join(d, "o.json")
    run([REALP, font, png, gt])
    run([OCR, "ocr", png, out], env={**os.environ, "LOAD": model,
                                     "CHARS": CHARS, "ASAN_OPTIONS": "detect_leaks=0"})
    gts = []
    for line in open(gt):
        m = re.match(r"line \d+: (.+)", line)
        if m: gts.append(m.group(1))
    try:
        ps = [b.get("text", "").strip() for b in json.load(open(out))["blocks"]]
    except Exception:
        ps = []
    c, t = score(gts, ps)
    shutil.rmtree(d, ignore_errors=True)
    return c, t, len(gts), len(ps)

def main():
    model = sys.argv[1]; font = sys.argv[2]
    seeds = [int(s) for s in sys.argv[3:]] or [1, 2, 3, 4, 5]
    cc = ct = bc = bt = 0
    for sd in seeds:
        c, t, gl, pl = eval_clean(model, font, sd)
        cc += c; ct += t; bc += gl; bt += pl
    rc, rt, rgl, rpl = eval_real(model, font)
    print(f"CLEAN  char-acc: {cc}/{ct} = {100*cc/ct:.1f}%  "
          f"({bt} blocks / {bc} GT lines)")
    print(f"REAL   char-acc: {rc}/{rt} = {100*rc/rt:.1f}%  "
          f"({rpl} blocks / {rgl} GT lines)")

if __name__ == "__main__":
    main()
