# GUI Human-Parity — WuBuPad / WuBuOffice (2026-08-11)

> This is the **honest GUI gap list**, replacing the misleading module-parity
> numbers. Module existence ≠ GUI human-parity. A rendered GUI whose chrome
> text doesn't draw has ~0% GUI parity regardless of how many modules link.

## The measurement: ratio-based, not module-count

`tooling/gui_parity.py` captures a real rendered frame and counts **legible
(bright) pixels per chrome band**. A band at ~0% bright-ratio is functionally
blind — no text is actually drawn there. This is a *rendered-output* measure,
immune to "the module links so it's done" logic.

Reference apps (Notepad++ / LibreOffice) chrome occupies ~10–25% of their
frame as legible text (menus, toolbars, tab strips, status bars). Our GUIs
were far below that.

## Measured (WuBuOffice shell, 960x720 frame)

| Chrome band | Before fix | After fix |
|---|---|---|
| Tab bar | 0.9% | **9.4%** |
| Menu bar | 0.0% | **3.4%** |
| Toolbar | 0.0% | **16.7%** |
| Status bar | 0.3% | **9.2%** |
| **Mean legibility** | **0.3%** | **9.7%** |

### The bug that made us "blind" (now fixed, commit 914e79be)
`sdl_text()` measured glyph width by calling `wuos_font_draw(fb=NULL)`, which
returns 0 when the framebuffer is NULL — so it bailed before drawing **any**
chrome text. Every menu/tab/toolbar/sidebar/status label was silently blank.
The document view used a *different* working font path, which is why only the
document text appeared and the bug hid through every vision-based "verify".
Fix: measure with `wuos_font_text_width()`.

**This is exactly why module parity (~90–100%) coexisted with a GUI that was
functionally 10–30% legible.** The GUI was effectively blind and we were
counting features, not looking at pixels.

## What "GUI parity" really needs (beyond modules)

The user's framing is correct: GUI parity is a *rendered, human-legible*
property. The real gaps (in priority order):

1. **Chrome must draw text** — DONE for WuBuOffice (above). Audit WuBuPad the
   same way (`shot` + `gui_parity.py`).
2. **Legible chrome band ratio** — our 9.7% mean is still below reference
   (~10–25%). Menu bar at 3.4% is thin; toolbar/status need larger, higher-
   contrast labels.
3. **Feature → rendered element** — each claimed feature must have a *visible*
   control: a menu item, a toolbar button, a tab, a sidebar entry, a status
   field. If it's only reachable via an undocumented hotkey, it's not GUI
   parity.
4. **Notepad++-specific (WuBuPad)** — the 10 real gaps from GAPS_NOTEPAD.md:
   indent guides, whitespace viz, call tips, style configurator, squiggly
   indicators, annotations panel, distraction-free, F11 fullscreen, multi-view
   split, more lexers.
5. **Office-specific (WuBuOffice)** — every engine must expose its document
   chrome (Navigator, zoom, word count, status) with legible labels, not just
   the Document view.

## How to re-audit (do NOT trust vision alone)

```sh
# WuBuOffice: capture a frame, then pixel-audit
cd /home/wubu/WuBuOffice
WUOS_DUMP=/tmp/wuos.ppm timeout 3 xvfb-run -a ./build/apps/wubuos/wubuos
ffmpeg -y -f image2 -i /tmp/wuos.ppm /tmp/wuos.png
python3 /home/wubu/tooling/gui_parity.py /tmp/wuos.png

# WuBuPad: shot tool writes raw RGBA+header; convert then audit
cd /home/wubu/WuBuPad
SDL_VIDEODRIVER=dummy ./build/shot /tmp/pad.raw /tmp/f.txt
python3 -c "import struct;from PIL import Image;d=open('/tmp/pad.raw','rb').read();w,h=struct.unpack('>HH',d[4:8]);Image.frombytes('RGBA',(w,h),d[8:],'raw').save('/tmp/pad.png')"
python3 /home/wubu/tooling/gui_parity.py /tmp/pad.png
```

Target: every chrome band ≥ ~10% legible ratio, and every claimed feature has
a visible, labeled control.
