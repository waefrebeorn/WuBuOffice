# GAPS_REAL — Triple Devil's Advocate Audit (2026-08-13)

Method: 3×DA (Truth / Value / Adversarial) over WuBuOffice + WuBuPad, executed by
building, running ctest, pixel-auditing the GUI, and reading the module source.
Not by trusting the docs. The docs said "100% parity / all green." They were wrong.

## Pass 1 — TRUTH (factual accuracy)

| Claim in GAPS_REAL.md | Verified reality | Verdict |
|---|---|---|
| "LibreOffice / OnlyOffice parity 100%" | Repo did NOT compile (`viewshot` target missing 32 module include/link lists + `hive.c`) | FALSE until fixed |
| "105 modules, 101 REAL" | A module counts REAL if linked + called once. 5 were hollow data-stores with no engine output | MISLEADING (linkage ≠ function) |
| "32 new REAL modules ... all green under ASan" | The modules existed but 5 had no computed output; tests only asserted "didn't crash" | WEAK (green ≠ correct) |
| WuBuPad "ctest 22/22 green" | Built + ran: 22/22 pass; GUI `gui_parity` pixel-audit PASSES | TRUE (honest) |

## Pass 2 — VALUE (structural integrity)

- WuBuPad: every module has a matching `test_*` AND the GUI pixel-audit passes →
  value is real. No action needed.
- WuBuOffice: `viewshot` (GUI capture, needed by `gui_parity`) could not build →
  the suite's own correctness gate was dead. Fixed by completing `viewshot`'s
  include/link lists + `hive.c`.
- 5 Office modules (wubu3d, wubutransition, wubuanimation, wubusmartart,
  wubuconnector) advertised engine-grade features (3D mesh engine, slide
  transitions incl. Morph, keyframe animation, diagram layouts, connectors) but
  implemented only the data model. Upgraded each to a real minimal engine with a
  behavioral test.

## Pass 3 — ADVERSARIAL (what breaks / what's dangerous)

- **Danger: the parity scanner conflates "called once" with "implemented."** Any
  future wave that adds `create/set/destroy` + a `doccmd_*_demo` counts as REAL
  without doing anything. Mitigation: tests must assert *output*, not linkage.
- **Danger: copy-pasted include/link lists across `wubuos`/`test_view`/`test_doccmd`/
  `viewshot` drift.** The 2026-08-11 wave updated three of four targets and left
  `viewshot` stranded. Any new TU compiled by all four is one omission from a
  broken build. (Ideal fix: factor the module lists into CMake variables; done
  as a localized patch to `viewshot` here to keep the change minimal + reviewable.)
- **Danger: vision hallucinates GUI text.** Confirmed pattern: a GUI can render
  0% legible chrome while a vision model "sees" full text. Verification MUST use
  `gui_audit.py` (pixel math), never a screenshot description. Both suites now
  wire this (`gui_parity` ctest).

## Remediation applied (this session)

1. `apps/wubuos/CMakeLists.txt`: `viewshot` gained the 32 wave-module
   include + link lists and `hive.c` source → it now builds.
2. `src/wubu3d/*`: +`wubu3d_rotate`/`wubu3d_project` (real perspective renderer).
3. `src/wubutransition/*`: +`wubutransition_progress` (per-frame blend factor).
4. `src/wubuanimation/*`: +`wubuanimation_progress` (eased timeline).
5. `src/wubusmartart/*`: +`wubusmartart_layout_boxes` (real layout pass).
6. `src/wubuconnector/*`: +`wubuconnector_route` (orthogonal L-route).
7. Each module's `test_*` upgraded to assert real behavior (not just "no crash").
8. `GAPS_REAL.md`: appended the truth-correction section above.

## Verification commands (re-run any time)

```sh
cd /home/wubu/WuBuOffice && cmake -B build -DWITH_SANITIZER=OFF . && cmake --build build -j4
cd build && ctest -R "wubu3d|wubutransition|wubuanimation|wubusmartart|wubuconnector|gui_parity" --output-on-failure
cd /home/wubu/WuBuPad && cmake -B build -DWITH_SANITIZER=OFF . && cmake --build build -j4
cd build && ctest -R "gui_parity|ui_gfx" --output-on-failure
```
