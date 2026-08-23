#!/bin/sh
# H4: shell accessibility tree must be valid JSON with roles+stable refs.
set -e
BIN="$(dirname "$0")/../build-plugin/apps/wubuos/wubuos"
SDL_VIDEODRIVER=dummy WUOS_A11Y_DUMP=/tmp/wb_a11y.json timeout 8 "$BIN" >/dev/null 2>&1 || true
python3 - <<'PY'
import json, sys
d = json.load(open("/tmp/wb_a11y.json"))
assert d["role"] == "application", "root role"
refs = []
def walk(n):
    if "ref" in n: refs.append(n["ref"])
    if n.get("role") == "tab": assert "selected" in n
    for c in n.get("children", []): walk(c)
walk(d)
tabs = [r for r in refs if r.startswith("tab:")]
menus = [r for r in refs if r.startswith("menu:")]
tbs = [r for r in refs if r.startswith("tb:")]
assert len(tabs) >= 5, f"tabs missing: {tabs}"
assert len(menus) >= 10, f"menu items missing: {len(menus)}"
assert len(tbs) >= 5, f"toolbar buttons missing: {len(tbs)}"
print(f"A11Y_TREE PASS ({len(refs)} elements: {len(tabs)} tabs, {len(menus)} menu items, {len(tbs)} buttons)")
PY
