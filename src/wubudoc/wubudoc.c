/* wubudoc.c -- unity aggregator for the unified document ingestion +
 * creation facade (wubuOS protocol surface).
 *
 * This file is intentionally tiny. All real logic lives in focused,
 * self-contained fragments under src/wubudoc/ so each concern (session/handle
 * state, kind detection, ingest, accessors, creation) is readable and
 * reviewable on its own:
 *
 *   doc_session  - DocHandle/DocSession state, create/free, id registry
 *   doc_detect   - extension + magic-byte kind detection, ZIP classification
 *   doc_ingest   - bytes/memory/path ingestors + normalized-model builder
 *   doc_access   - handle accessors + drop-text projection
 *   doc_create   - serialize a handle back out to any supported format
 *
 * The full (opaque-to-callers) DocHandle/DocSession structs plus the module
 * include block and the shared static forward declarations live in
 * wubudoc_internal.h (include-guarded), so each fragment is self-contained
 * and the shared helpers (session_add, build_model, ingest, collect_parts,
 * ...) are defined exactly once. The facade reuses clean-room sub-libraries
 * (wubujson, wubufont, wubuzip, wubucfb, wubuconv, ...) and never re-implements
 * format logic. See wubudoc.h for the public API. The NDJSON agent lives in
 * agent.c (a thin dispatcher over this API).
 */
#include "wubudoc_internal.h"

#include "doc_session.inc"
#include "doc_detect.inc"
#include "doc_ingest.inc"
#include "doc_access.inc"
#include "doc_create.inc"
