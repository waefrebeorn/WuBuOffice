# GUI Mathematics — 10 Paradigms × 10 Aspects = 100 Principles

Research synthesis (2026-08-12, 25+ searches across 6 Kevin-Bacon hops).
Every GUI principle expressible as a number, formula, or ratio, grouped into
10 paradigms of 10 aspects each. Source-grounded (NN/g, Laws of UX, W3C, WebAIM,
Apple HIG, Material, Atlassian, USWDS, iXDF, Wikipedia).

---

## Paradigm 1 — Laws of UX (cognitive performance)
1. Fitts's Law: MT = a + b·log₂(2D/W); index of difficulty ID = log₂(2D/W) bits.
   Bigger targets, closer targets = faster. → hit-target size, distance.
2. Hick's Law: T = b·log₂(n+1). Fewer choices = faster decisions. → menu item count.
3. Miller's Law: working memory holds 7±2 chunks. → group info into ≤7 items.
4. Tesler's Law: complexity is conserved; shift it to the app, not the user.
   → smart defaults, hidden advanced options.
5. Von Restorff / Isolation Effect: the one item that differs is remembered.
   → accent color for the primary action only.
6. Jakob's Law: users expect the conventions of other products. → standard
   menu placement, Ctrl+C/V/X, F-keys.
7. Serial Position Effect: primacy + recency — first and last list items
   remembered best. → put key actions first/last in menus, middle for hidden.
8. F-pattern / Z-pattern scanpath: eyes scan top-left→bottom in a Z/F.
   → primary content top-left, CTA top-right.
9. Doherty Threshold: respond <400ms to keep flow. → instant feedback.
10. Parkinson's Law: work expands to fill time; tasks feel proportionate to
    effort. → show progress, break big ops.

## Paradigm 2 — Psychophysics (perception math)
1. Weber's Law: JND = k·I; just-noticeable difference is proportional to
   magnitude. → spacing/contrast deltas must exceed the JND at that size.
2. Weber–Fechner Law: perceived = k·log(I/I₀). → logarithmic volume/size scales.
3. Stevens' Power Law: perceived = a·I^b (b differs per sense; luminance ~1/3).
   → brightness/size mapping is a power function, not linear.
4. CIELAB ΔE (delta-E): perceptual color difference ≈ Euclidean distance in
   L*a*b*. → distinguish UI states by ΔE ≥ 1 (JND) or ≥ 2 (noticeable).
5. Perceptual uniformity: CIELAB designed so equal ΔE = equal perceived diff.
   → pick colors from a perceptually-uniform space, not RGB.
6. Absolute threshold: minimum stimulus perceivable. → minimum readable font.
7. Contrast sensitivity: falls with size; small text needs more contrast.
   → WCAG 4.5:1 normal, 3:1 large.
8. Brightness constancy: perceived brightness relative to surround.
   → dark mode needs its own palette, not inverted.
9. Simultaneous contrast: adjacent colors shift perceived color.
   → check color pairs, not isolated swatches.
10. Flicker fusion threshold: ~60Hz perceived as continuous. → 60fps target.

## Paradigm 3 — Gestalt (perceptual grouping)
1. Proximity: near = grouped. → spacing codes grouping (8pt grid gaps).
2. Similarity: same color/shape/size = grouped. → consistent styling per type.
3. Closure: open shapes read as closed. → fill in outlines, implied boxes.
4. Prägnanz / Good Figure: simplest organization wins. → minimalism, no noise.
5. Figure/Ground: distinguishable foreground vs background. → contrast, elevation.
6. Continuation: smooth lines group. → aligned elements read as one path.
7. Symmetry & Order: symmetric reads as stable. → balanced layout.
8. Common Region: elements in one closed region group. → cards, panels, borders.
9. Common Fate: elements moving together group. → synchronized animation.
10. Uniform Connectedness: connected elements group. → connectors, lines.

## Paradigm 4 — Typographic scales (ratio)
1. Minor Second 1.067, 2. Major Second 1.125, 3. Minor Third 1.200,
   4. Major Third 1.250, 5. Perfect Fourth 1.333, 6. Augmented Fourth 1.414,
   7. Perfect Fifth 1.500, 8. Golden Ratio 1.618.
9. Modular scale rule: font sizes = base × ratio^n (one ratio per ladder).
10. Size hierarchy: each level distinct (ratio ≥ minor third) → clear H1..body.

## Paradigm 5 — Spacing scale (8pt grid)
1. Base unit 4px; 2. 8px standard; 3. scale: 8,16,24,32,40,48,56,64,72,80;
   4. every gap/padding/margin = base × integer; 5. component heights on grid;
   6. icon sizes 16/20/24/32 (multiples of 4); 7. line-height multiple of grid;
   8. tight spacing uses 4px (half-base); 9. large spacing 64/80/96;
   10. consistency: same gap = same relationship everywhere.

## Paradigm 6 — Proportion & layout (geometry)
1. Golden ratio 1.618; 2. Rule of thirds 1:1:1; 3. Phi grid 1:1.618:1;
   4. Golden rectangle 1:1.618; 5. Aspect 4:3 (1.333); 6. Aspect 3:2 (1.5);
   7. Aspect 16:9 (1.778); 8. Dynamic symmetry (root rectangles √2, √3, √5);
   9. Content:sidebar = golden (NN/g: 960→content 593, sidebar 367);
   10. Focal points at third/phi intersections.

## Paradigm 7 — Color (proportion + perception)
1. 60-30-10 rule: 60% dominant, 30% secondary, 10% accent.
2. WCAG AA contrast 4.5:1 normal / 3:1 large; 3. AAA 7:1 / 4.5:1.
4. Semantic tokens: primitive palette → semantic role → component.
5. Dark mode: lighten surfaces, darken text, test each color vs bg.
6. Accent for ONE thing (Von Restorff); 7. Danger/warn/success consistent hue.
8. ΔE ≥ 2 to distinguish states; 9. Hue consistency within a role;
   10. No color-alone signaling (add icon/text).

## Paradigm 8 — Attention & hierarchy (salience)
1. Size: bigger = more attention; 2. Weight: bold = stronger;
   3. Position: top-left first; 4. Isolation: contrast draws eye;
   5. Signal-to-noise: maximize relevant, cut noise;
   6. Visual weight balance; 7. Scanpath: 3-5s to first fixations;
   8. One primary CTA per view; 9. Hierarchy steps distinct (type scale);
   10. Progressive disclosure: surface detail on demand (Tesler).

## Paradigm 9 — Timing & motion (time math)
1. Doherty <400ms; 2. RAIL: frame 16ms, respond 100ms;
   3. 60fps = 16.7ms/frame; 4. Ease-out for entrances, ease-in-out for looping;
   5. Duration scale 150/300/500/700ms; 6. cubic-bezier easing curves;
   7. Spring physics (stiffness/damping) for natural motion;
   8. Skeleton/optimistic UI for perceived speed;
   9. No motion > 200-400ms without feedback; 10. Consistent easing/duration.

## Paradigm 10 — Usability heuristics (Nielsen's 10)
1. Visibility of system status; 2. Match system & real world;
3. User control & freedom (undo); 4. Consistency & standards (Jakob);
5. Error prevention; 6. Recognition over recall; 7. Flexibility & efficiency
(shortcuts); 8. Aesthetic & minimalist design (Prägnanz/signal-noise);
9. Help users recognize & recover from errors; 10. Help & documentation.

---

## What's measurable & already gated (tooling/)
- Contrast → wcag_palette.py (Paradigm 2/7)
- Type scale, line-height, spacing grid, golden layout, hit-target →
  design_ratios.py (Paradigm 4/5/6)
- Legibility, glyph height, chrome presence → gui_audit.py / tricorder.py

## Next encodable gates (apply to WuBuPad + WuBuOffice)
- Hick's Law: menu/toolbar item count ≤ 7 visible (or grouped). [P1]
- Miller's 7±2: per-view distinct chunks ≤ 9. [P1]
- Fitts: hit-target ≥ 24px already; raise toolbar/tab to 40px+ for comfort. [P2]
- 60-30-10 color proportion (measure accent vs neutral coverage). [P2]
- Serial position: primary actions first/last in menus (audit order). [P2]
- Doherty: check feedback latency paths. [P3]
- ΔE between state colors ≥ 2. [P2]
