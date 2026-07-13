# WuBuOffice

A **ground-up C11 reimplementation** of an open-document office suite.

> **SLERM** (verb): take someone's full work and build your own version from
> scratch. Not a fork — a clean-room, ground-up reimplementation.

WuBuOffice is a SLERM of the Microsoft Office / OOXML ecosystem. We read the
*reference* (the open OOXML/ECMA-376 format and the reference SDKs) but write
every byte ourselves in C11 — no .NET, no Java, no forks.

## Status

This is the foundation: a working OOXML **container + writer** pipeline that
already produces a real, openable `.docx`.

| Layer | Status | Description |
|-------|--------|-------------|
| `wubuzip` | ✅ working | From-scratch ZIP writer (store mode) + IEEE-802.3 CRC-32 |
| `wubuxml` | ✅ working | Streaming, well-formed XML writer with correct escaping |
| `wubuoxml` | ✅ working | OPC package writer — `[Content_Types].xml` + `.rels` graph |
| `wubuword` | ✅ working | CLI that emits a valid WordprocessingML `.docx` |

Next: `wubucell` (xlsx), `wubushow` (pptx), and richer WordprocessingML
(paragraph styles, tables, images, numbering).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires a C11 compiler (tested: gcc 13.3, clang). POSIX `open_memstream`/
`strdup` are used via `_POSIX_C_SOURCE=200809L` (no GNU extensions).

## Use

```sh
./build/wubuword out.docx "My Title" "My body text"
```

`out.docx` opens in Microsoft Word and LibreOffice.

## Layout

```
src/wubuzip/   ZIP container core (zip.h / zip.c)
src/wubuxml/   XML writer       (xml.h / xml.c)
src/wubuoxml/  OPC package       (package.h / package.c)
apps/wubuword/ the docx generator (main.c)
tests/         unit tests (ctest)
docs/          format notes pulled from the reference material
```

## Why "C11 or bust"

Office formats are byte-level container formats. A from-scratch C11 core keeps
the runtime at zero dependencies, is trivially embeddable (games, firmwares,
servers), and is the smallest honest surface to reimplement a format on. No VM,
no runtime, no telemetry.

## References consulted (not copied)

- ECMA-376 / ISO 29500 (Office Open XML)
- `dotnet/Open-XML-SDK` — the reference .NET SDK (read for format truth)
- OPC spec (ISO 29500-2 packaging)

WuBuOffice's code is original. No file from any upstream repo is compiled,
linked, or vendored here.
