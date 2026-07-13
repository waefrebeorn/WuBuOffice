# SLERM vs Fork vs Vendoring

WuBuOffice is a **SLERM**. Definitions, so nobody confuses the posture:

| Approach | Copy code? | Compile upstream? | From scratch? |
|----------|-----------|-------------------|---------------|
| Fork | yes (modifies it) | yes | no |
| Vendor | yes (static copy) | yes (linked in) | no |
| **SLERM** | **no** | **no** | **yes (clean room)** |

## What we take from upstream

We read the *reference* repositories only to learn **format truth**:
- `dotnet/Open-XML-SDK` — confirms element/attribute names, content types,
  relationship type URIs.
- ECMA-376 / ISO 29500 — the normative schema.
- OPC (ISO 29500-2) — packaging rules.

We then write our own parsing/serialization in C11.

## Hard rule

No `.cs`, `.java`, `.csproj`, or upstream source file is present in this repo.
If you add one, it stops being a SLERM. Keep it C11, keep it original.
