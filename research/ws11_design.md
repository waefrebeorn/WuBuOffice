# WS11 — Unified Object Model (Design)

**Status:** design + green-baseline core (compiling `src/wubumodel`).
**Role:** the integration keystone for the whole suite. WS13 (inference),
WS14 (RL), WS15 (OS), WS16 (cross-app), WS17 (knowledge) all hang off this.

## Why a single model
Today the three apps each own a private structure (`wubucell_book`,
`wubuword_doc`, `wubushow_pres`). That blocks every integration advantage:
- AI (WS13) can't read "the document" uniformly.
- RL (WS14) can't encode state across apps.
- Cross-app live objects (WS16) need a shared node identity.
- The OS (WS15) and knowledge store (WS17) need one queryable surface.

The docket item **"Define one canonical object model shared by all three
apps"** is the #1-ranked WS11 item and the precondition for D/C-style moves.

## Principles (from docket WS11)
1. **One canonical model** shared by all three apps.
2. **Every entity is a typed node** (`run`, `cell`, `shape`, `paragraph`, ...).
3. **Separate content, style, and layout.** Layout is *derived* (computed at
   render time), never stored in the canonical model → the model is UI-free
   and headless-testable (docket: "Keep the model free of UI concepts").
4. **Stable IDs** on every node → diff/merge/undo/collab.
5. **Command pattern** → all edits reversible and recordable (enables undo,
   RL episode replay, collaboration patches).
6. **Reactive** → UI updates only from model-change events.
7. **Serializable** to our internal format **and** OOXML (I/O is separate;
   the core model has no file code).
8. **Incremental patches** → collaboration sync is a stream of node commands.
9. **Typed extension** → plugins/extensions (WS12) declare new node kinds.
10. **Semantic layer** → nodes carry enough meaning for WuBuMath (WS13) and
    the knowledge graph (WS17) to read.

## Node taxonomy (v1)
```
DOC        – top container (a workbook / a word doc / a deck is a DOC)
SECTION    – a logical division (sheet, chapter, slide)
BLOCK      – paragraph / table / shape container
PARAGRAPH  – a run container
RUN        – inline content (text, with style)
CELL       – a grid cell (holds a value or formula)
SHAPE      – vector/box/image primitive
CHART      – chart node (links to a CELL range)
TABLE      – structured table (references ranges)
FIELD      – computed field (TOC, page #, cross-ref)
LINK       – a link to another node (cross-app live object, WS16)
```

## Memory layout (opaque)
- `wubumodel_doc` owns a node table (id → node) and a style table.
- `wubumodel_node` is a tagged union over the taxonomy above; `kind` selects
  the payload. `style` is a *shared* `wubumodel_style*` (COW).
- IDs are monotonic `uint64` issued by the doc; stable for the doc's life and
  preserved across save/load (so a re-opened doc keeps node IDs → merges work).

## Command / undo
```
wubumodel_cmd { kind; target_node_id; before; after; }  // value-blob pair
apply(cmd)   -> mutates node, pushes inverse to undo stack
undo()       -> replays inverse
```
A `wubumodel_txn` batches commands so cross-app drag (WS16) is one undo entry.

## Observers (reactive)
`wubumodel_on_change(doc, cb)` fires on any committed command → UI/OS/indexer
(WS15)/knowledge-extractor (WS17) subscribe. No polling.

## Collaboration (WS06) hook
Because edits are commands with target IDs, a CRDT layer can stream
`wubumodel_cmd` between peers; conflict resolution operates on node IDs. This
is the seam WS06 plugs into — the model doesn't implement CRDT, it *emits*
commands CRDT can carry.

## Inference (WS13) / RL (WS14) hook
- Inference reads nodes via the read-only accessor API (no mutation) → safe.
- RL encodes `state` from a node subtree and emits `wubumodel_cmd` as actions;
  the safety wrapper (docket WS14/WS18) validates before `apply`.

## Files
- `src/wubumodel/model.h` — public opaque API (no struct defs leaked).
- `src/wubumodel/model_internal.h` — struct defs (app/extension code can't
  include this; enforces encapsulation, per no-monolith rule).
- `src/wubumodel/model.c` — core impl (create/destroy, node CRUD, commands,
  observers).
- `src/wubumodel/style.c` — shared style table (COW).
- `tests/test_model.c` — unit + command/undo smoke tests.

## Build status
Green baseline: doc create, node create with stable id, style attach,
`set_text` command + undo, observer fires. Later PRs layer OOXML I/O, the
typed payloads per app, and the CRDT seam.
