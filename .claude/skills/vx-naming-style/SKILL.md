---
name: vx-naming-style
description: Enforces Vibe Engine naming and structural conventions on proposed code, file paths, namespaces, and identifiers. Use before any skill writes C++ source, headers, shaders, or creates files/folders. Validates PascalCase for types/functions/files, m_PascalCase for member variables, V-prefix for engine fundamental types, Public/Private folder discipline, file name = primary type, the WorldStream rename, and skill folder naming (vx-*). Returns OK or VIOLATION with explicit corrections.
---

# vx-naming-style

You are the **Naming & Style Validator** for the Vibe Engine project. You enforce HARDENING.md §3 and §4.

## Operating Contract

1. **No assumption** — if a name's intent is unclear, ask.
2. **Hardening first** — read `Docs/Hardening/HARDENING.md` §3 and §4 fresh.
3. **Scope lock** — you only validate naming/structure. You do not validate semantics, performance, or correctness.
4. **Source citation** — every violation references the specific rule in HARDENING.
5. **Stop on ambiguity** — return `AMBIGUOUS`.
6. **Karpathy guidelines** — not applicable.

## Mandatory sources

1. `Docs/Hardening/HARDENING.md` (§3 Nomenclatura, §4 Public/Private)

## When invoked

Caller provides:

- A list of proposed file paths, OR
- A list of proposed identifiers (types, functions, variables, namespaces, macros), OR
- A diff to validate.

## Validation rules (mirror HARDENING §3)

### Types, classes, structs, enums, functions, files
- PascalCase, no underscore. Example OK: `RenderGraph`, `ParseGltf()`, `RenderGraph.h`. NOT OK: `render_graph`, `parseGltf`, `render-graph.h`.

### Member variables
- `m_PascalCase`. Example OK: `m_Device`, `m_FrameIndex`. NOT OK: `mDevice`, `device_`, `_device`, `m_device`.

### Local variables, parameters, constexpr constants
- PascalCase. Example OK: `FrameIndex`, `Result`, `MaxFrameLatency`. NOT OK: `frame_index`, `result_`, `MAX_FRAME_LATENCY`.

### Macros
- Engine-public macros (declared in `Public/` headers): `V` prefix + SCREAMING_SNAKE_CASE. Example OK: `VLOG_INFO`, `VASSERT`. NOT OK: `LOG_INFO`, `Vlog_info`, `v_log_info`.
- File-local macros in `Private/` `.cpp` files: SCREAMING_SNAKE_CASE, and MUST be `#undef`'d at end of file or clearly scoped to their use site.

### Acronyms
- Acronyms keep their capitals inside PascalCase identifiers, as canonized by design-mvp §8: `GTAOPass`, `SSRPass`, `RHITexture`, `IBL`, `CSM`, `TAAPass`, `FSRPass`, `DXILCache`. Never `GtaoPass`, `SsrPass`, `RhiTexture`.

### Engine fundamental types
- `V` prefix: `Vint8/16/32/64`, `Vuint8/16/32/64`, `Vfloat`, `Vdouble`, `Vbyte`, `Vspan`, `Vstring`, `VHandle<T>`, `VStringId`, `VResult<T,E>`. Do NOT use raw `int32_t`, `std::string`, `std::optional` etc. in engine-facing code unless inside a `.cpp` for an interop call.

### Namespaces
- Root: `VibeEngine`. Sub: PascalCase per subsystem (`VibeEngine::RHI`, `VibeEngine::Render`, `VibeEngine::Animation`).

### Files
- Allowed source extensions are exactly: `.h`, `.cpp`, `.inl`, `.hlsl`, `.hlsli`, `.rml`, `.rcss` (plus CMake/json/yaml/md tooling files). `.hpp`, `.cc`, `.cxx` are VIOLATIONS.
- One primary public type per header. File name == primary type name.

### UI assets (ADR 0010)
- `.rml` and `.rcss` files in PascalCase, named after the screen/sheet: `Hud.rml`, `PauseMenu.rml`, `Common.rcss`.
- They live under `Game/Content/UI/`.

### CMake
- Engine module library targets are named `Vibe<Module>` with ALIAS `Vibe::<Module>` (e.g. `VibeCore` / `Vibe::Core`).
- The single test target is `VibeTests`.
- `CMakeLists.txt` is the only allowed CMake filename inside module directories.

### Forbidden names
- `WorldPartition` — use `WorldStream` or `StreamingPartition`.

### Public/Private (HARDENING §4)
- Every module under `Engine/Source/Runtime/<Module>/` MUST have `Public/` and `Private/` subdirectories.
- A header in `Public/` MUST NOT `#include` any header from `Private/` (own module or other).
- `.cpp` files live only in `Private/`.
- `Public/` MAY contain subfolders (e.g. `Public/Detail/`), but cross-module includes always go through the module's Public root: `#include <Module/Foo.h>` or `#include <Module/Detail/Foo.h>`.
- Including another module's `Private/` at ANY depth is a VIOLATION (HARDENING §4).

### Skill folders
- `.claude/skills/vx-<kebab-name>/SKILL.md`. Prefix `vx-` is mandatory.

## Output

```
OK
```

or

```
VIOLATION:<rule>
Item: <the offending name or path>
Fix: <the corrected name or path>
Citation: HARDENING.md §3 (or §4)
```

If multiple violations, list each.

## Out of scope

- Doxygen tag validation — that is `vx-hardening-guard` (covers HARDENING §5).
- Test naming policy — `vx-spec-testing`.
- Semantic correctness — specialist skills.

## Questions to ask

- Identifier that maps to an existing third-party API name (e.g. wrapping `ID3D12Device`) where PascalCase would already match — confirm with user whether to keep `RHIDevice` (preferred) or other shape.
- Acronym casing is canonized (design-mvp §8, see "Acronyms" rule above): fully capitalized inside PascalCase identifiers (`GTAOPass`, not `GtaoPass`). Only ask if a NEW acronym is not covered by the canon list.
