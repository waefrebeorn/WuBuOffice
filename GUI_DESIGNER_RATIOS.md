# Designer Ratios — Research Synthesis (2026-08-12, 20+ searches)

Contrast (WCAG) is ONE ratio. Designer systems use a full set of mathematical
ratios for hierarchy, spacing, proportion, and rhythm. This documents the
complete set from research, for WuBuPad + WuBuOffice.

## 1. Typographic scales (musical intervals; Bringhurst, typescale.com, cieden)
Font sizes form a geometric series from a base × ratio^n. Ratios (musical):
- 1.067 Minor Second, 1.125 Major Second, 1.200 Minor Third, 1.250 Major Third
- **1.333 Perfect Fourth** (most common for UI), 1.414 Augmented Fourth
- 1.500 Perfect Fifth, **1.618 Golden Ratio** (highest contrast)
Rule: a consistent type scale uses ONE ratio across the size ladder. Steps
between heading sizes should be ≈ ratio^n × base.

## 2. Spacing scale (Atlassian, USWDS, 8pt grid)
Base unit 4px or 8px; all spacing = base × integer. 8pt grid: 8,16,24,32,40,48,
56,64,72,80. 4pt for tight. Rule: every padding/margin/gap is a multiple of the
base unit.

## 3. Golden ratio layout proportion (NN/g, iXDF)
Content : sidebar = **1.618 : 1**. For a 960px app: content ≈ 593, sidebar
≈ 367. Rule: main area is 1.618× the side panel.

## 4. Vertical rhythm / line height (imperavi, gridmakerpro)
Body line-height ratio ≈ **1.5–1.6** (line_h = font_size × 1.5). Baseline grid:
all vertical spacing aligns to the leading unit. Rule: line_h/font ≈ 1.5.

## 5. Hit target size (WCAG 2.5.8, Apple HIG, Material)
- **24×24px minimum** (WCAG 2.5.8 Level AA)
- **44×44px** (Apple), **48×48dp** (Material) for comfortable tapping
Rule: every interactive element ≥ 24px; prefer ≥ 40px for mouse/pointer.

## 6. Icon / component size grid (dutchicon, 8pt)
Icons render at 16/20/24/32 (multiples of 4 or 8); 1px grid for sharpness.
Rule: icon + component sizes are multiples of 4.

## 7. Aspect ratios (Wikipedia)
3:2 (1.5), 4:3 (1.333), 16:9 (1.778), 1:1. Pages/slides/cells align to a
classical aspect ratio, not arbitrary dimensions.

## 8. Composition grids (rule of thirds, phi grid)
Rule of thirds = 3×3 equal (1:1:1). Phi grid = golden divisions (1:1.618:1).
Used for frame/focus placement.

## Application to WuBuPad + WuBuOffice
Gate each rendered dimension against the nearest designer ratio:
- font sizes in a modular scale (base 16 or 20 × ratio)
- line_h/font ≈ 1.5
- chrome spacings multiples of 4 (8pt grid)
- content:sidebar ≈ 1.618
- interactive targets ≥ 24px (WCAG 2.5.8)
- page aspect ratios near classical values

## Priority
1. Type-scale conformance (hierarchy is the highest-visibility ratio)
2. Spacing grid (8pt) — chrome heights/gaps
3. Line-height ≈1.5 (vertical rhythm)
4. Golden-ratio content:sidebar
5. Hit-target ≥24px
