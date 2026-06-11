# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Mandatory reading order before any action

1. [Docs/Hardening/HARDENING.md](Docs/Hardening/HARDENING.md) — non-negotiable rules. Read fresh before every operation.
2. [Docs/design-mvp.md](Docs/design-mvp.md) — MVP contract.
3. This file.

**No action outside a `vx-*` skill.** The project enforces a strict workflow: phase analysis → task creation → task execution. Every step happens through a skill that consults the hardening and the design doc. Free-form coding, refactoring, or "while we're here" cleanup is forbidden. If the user requests something that does not map to an existing skill, ask before acting.

**Exception — curated auxiliary skills.** `vx-*` orchestrators MAY delegate to a curated set of Matt Pocock skills (`tdd`, `diagnose`, `grill-with-docs`, `to-issues`) as internal sub-steps, governed by [Docs/References/MATT-SKILLS-BINDING.md](Docs/References/MATT-SKILLS-BINDING.md). These auxiliary skills never run standalone (they carry `disable-model-invocation: true`), are always translated to project conventions (Catch2/`VibeTests.exe`, PT-BR artifacts, `Docs/Decisions`, `Docs/Context/GLOSSARY.md`), and never widen a task's scope. Do **not** run `setup-matt-pocock-skills` — it would fork the project's conventions.

## Project status

The MVP contract is to deliver two executables sharing one runtime:

- `VibeEditor.exe` — opens the project, viewport, asset browser, inspector, hierarchy, console, profiler, playmode.
- `VibeGame.exe` — loads one level, third-person character, melee combat vs. one enemy, PBR rendering, physics, audio.

When the user asks to implement something, first check which phase (§9 of the design doc) the work belongs to and confirm phase ordering before jumping ahead — fases are sequenced deliberately.

## Planned stack (from §4 of design-mvp.md)

| Area | Choice |
|---|---|
| Language | C++23 |
| Build | CMake 3.28+ presets, Ninja, vcpkg (baseline-locked) |
| Compiler | MSVC (Visual Studio 2022) |
| Graphics | DirectX 12 (Agility SDK), HLSL SM 6.6+ via DXC |
| Math | glm (wrapped) |
| Physics | Jolt |
| Audio | miniaudio |
| Mesh import | cgltf, ufbx |
| Texture | DirectXTex, stb_image, BC1–BC7 |
| UI editor + debug | Dear ImGui (D3D12 + Win32 backends) |
| UI do jogo (HUD) | RmlUi via módulo `EngineUI` (RenderInterface over the RHI — ADR 0010) |
| JSON / YAML | nlohmann/json, yaml-cpp |
| Logging / profiling / testing | spdlog, Tracy, Catch2 |
| Upscaling | AMD FSR 2/3 (FidelityFX SDK) |
| GPU memory | D3D12MA |

## Build / run

No build system exists yet. Once Fase 1 (§9) lands, builds will use CMake presets + Ninja + vcpkg, producing `VibeEditor.exe` and `VibeGame.exe` under `Build/`. **Do not invent build, test, or lint commands** — confirm the actual toolchain state with the user before running anything.

The full-build acceptance criterion (§3) is: clean clone → Visual Studio 2022 + CMake + vcpkg → working build in ≤30 minutes, no manual steps.

## Planned module layout (§6)

```
Engine/Source/Runtime/    Core, Platform/Windows, Math, Memory, JobSystem, FileSystem,
                         RHI/D3D12, Renderer (+ RenderGraph, Passes, Lighting,
                         Shadows, PostProcess), ShaderSystem, MaterialSystem,
                         TextureSystem, MeshSystem, AssetSystem, World, ECS,
                         Animation, Character, Physics, AI, Gameplay, Combat,
                         Audio, Input, Camera, UI, SaveGame
Engine/Source/Editor/    EditorApp, Viewport, AssetBrowser, Inspector, Hierarchy,
                         Console, Profiler
Engine/Source/Tools/     AssetImporter
Engine/Shaders/          Common, Mesh, Materials, Lighting, Shadows, PostProcess
Game/Source/GameRuntime/
```

Every module enforces `Public/` (exposed headers) vs. `Private/` (implementation) separation. This is non-negotiable — §6 calls it out specifically to prevent the "include cake" anti-pattern.

## Naming conventions (§8.1, §5.11)

- **`V` prefix on engine types and macros**: `Vint32`, `Vuint64`, `Vfloat`, `Vbyte`, `Vspan`, `Vstring`, `VHandle<T>`, `VStringId`, `VResult<T,E>`, `VLOG_INFO/WARN/ERROR`, `VASSERT`, `VVERIFY`, `VCHECK`.
- **`WorldPartition` is renamed** to `WorldStream` or `StreamingPartition` — the Unreal name is a trademark. Do not reintroduce it.
- Module path matters: a type lives under the smallest module that owns it (e.g., `Handle<T>` is Core, `RHITexture` is RHI).

## Scope discipline (this is the contract)

The design doc is explicit (§10, §12): **anything not in §2 in-scope goes to a `PostMVP_Backlog.md`, with no exceptions until the MVP closes.** When proposing work, do not "helpfully" suggest features from the long-term design that the MVP deliberately deferred. Examples to *not* propose during MVP:

- Vulkan, console backends, mesh shaders, ray tracing, DDGI, DLSS/XeSS
- C# + Avalonia editor, reflection generator (libclang)
- Cooked binaries, `.vpkg`, DirectStorage, world streaming, HLOD
- Behavior trees, utility AI, motion matching, full-body IK
- Quests, dialogue, inventory, networking, multiplayer
- Wwise/FMOD, NoesisGUI
- C++ hot reload, Crashpad, save versioning, plugins

If the user is clearly asking for one of these anyway, flag the MVP deviation explicitly before proceeding.

**UI boundary (ADR 0010, HARDENING §2 "Fronteira de UI")**: RmlUi is in-scope but ONLY inside the `EngineUI` module (game HUD + menus). The editor stays 100% Dear ImGui + ImGuizmo; proposing RmlUi in the editor, or Dear ImGui in game product UI (debug overlays excepted), is a violation.

## Pragmatic MVP-only deviations from the long-term design (§5)

These exist because the long-term design is more ambitious. Future-you may be tempted to "do it right" — don't, the migration path is already planned:

1. **Editor in Dear ImGui**, not C# + Avalonia. Same editor services, different frontend later.
2. **No reflection generator** — manual `TypeRegistry` registration + Boost.PFR. Generator feeds the same registry post-MVP.
3. **No cooker / no `.vpkg`** — loose files (`.gltf`, `.dds`, `.json`) loaded on demand. `AssetHandle` is opaque, backend swaps later.
4. **Worker pool, not fibers** — same `JobSystem` API; fiber backend slots in if ever needed.
5. **RHI is multi-backend-shaped, but only D3D12 is implemented.** Don't write code that hardcodes D3D12 in non-D3D12 modules.
6. **HLSL + DXC, not Slang.** DXC already targets both DXIL and SPIR-V.
7. **Game HUD via RmlUi from the MVP** (ADR 0010 revised the old ImGui-HUD plan). `EngineUI` module isolates the backend; `.rml`/`.rcss` docs + data bindings; no future backend swap planned — post-MVP evolution happens inside RmlUi. NoesisGUI stays banned.
8. **miniaudio**, not Wwise/FMOD. `EngineAudio` interface isolates the backend.
9. **Whole level loaded at once**, no streaming. `LoadLevelAsync` API exists anyway so the backend can change.
10. **Single-slot save**, header-versioned binary, no migration logic.
11. **`WorldPartition` → `WorldStream`/`StreamingPartition`** (trademark avoidance).

## Implementation phases (§9)

Each fase ends with a verifiable, demonstrable build. Don't skip ahead.

1. **Foundation** — CMake/vcpkg, Core, Platform, Math, Memory, JobSystem, FileSystem, Catch2 baseline.
2. **RHI D3D12** — device, swapchain, bindless heap, DXC + DXIL cache; first textured quad with HLSL hot reload.
3. **Render Graph + ImGui** — transient resources, barrier solver, Tracy overlay.
4. **Realistic renderer** — GBuffer, deferred lighting, CSM, IBL, GTAO, SSR, TAA, FSR, ACES tonemap.
5. **Asset system** — glTF/FBX/PNG/EXR import, material JSON, file-watcher hot reload.
6. **World / ECS / Scene** — entity registry, components, JSON scene serialization, frustum culling.
7. **Animation + Character** — skeleton, clips, state machine, root motion, third-person camera.
8. **Physics (Jolt)** — `CharacterVirtual`, raycast/sweep/overlap, ragdoll on death.
9. **Combat + AI** — hitbox/hurtbox, attack data (JSON), combo graph, lock-on, single-enemy state machine.
10. **Audio** — miniaudio, spatial emitters, footsteps-by-surface, combat music crossfade.
11. **Editor** — dockable ImGui, viewport via `ImGui::Image`, ImGuizmo, manual `TypeRegistry`, PlayMode.
12. **Game executable + polish** — `VibeGame.exe`, EngineUI/RmlUi HUD + pause menu, save slot, Debug/Development/Shipping profiles.

The acceptance bar is §3: 60 fps at 1080p on RTX 3060 / RX 6600 with the full MVP feature list working end-to-end. Additionally, §3.1 sets a **design ceiling** (ADR 0011): the architecture must sustain 4K@60 via FSR on RTX 4070-class GPUs — never a gate, but it vetoes architecture that would make it impossible. §3.2 sets the memory-safety pillars (RAII, no raw new/delete, zero-leak smoke gates — HARDENING §12).

## Document language

The design doc is in **Portuguese**. The user works bilingually — quoting Portuguese terms back at them (e.g., "fase", "escopo", "critério de aceitação") is fine and often clearer than translating.

Generated artifacts (tasks, PRs, ADRs, phase docs, HARDENING.md) are in **PT-BR**. SKILL.md files themselves are in **EN** (for triggering accuracy), but their generated outputs are in PT-BR.

## Workflow & skills (`.claude/skills/vx-*`)

All work flows through the project skills. There are 20 of them, project-scoped, all prefixed `vx-`.

### Canonical flow

```
/vx-phase-analyze    →  produces Docs/Roadmap/Phases/Fase-NN-name.md (formato 2: cross-task contracts, canonical commands, machine-checkable acceptance, risk flags)
/vx-task-create      →  produces Docs/Roadmap/Tasks/NN-name.md (formato 2, self-contained)  +  updates Docs/Roadmap/README.md
/vx-task-execute     →  mechanical execution: format gate → read budget → deps → contract pre-flight → tdd RED list → exact validation gate → commit/push/PR
```

**Model split (ADR 0013, HARDENING §14)**: `vx-phase-analyze` and `vx-task-create` run on a large model (Opus-class) and do ALL the thinking — specialists are consulted at **creation time** and their contracts/test plans/commands are baked into the task doc. `vx-task-execute` runs on a smaller-context model (Sonnet-class) and is near-mechanical: it reads ONLY the task doc + its `contexto:` budget, never re-consults specialists, and halts with a structured fault report on any contradiction. Tasks without `formato: 2` in the frontmatter are v1 and must be rebaked before execution. Create tasks just-in-time (1–3 ahead), not a whole fase at once.

Every skill begins by reading `Docs/Hardening/HARDENING.md` (now §1–§14, including §12 memory safety, §13 performance budget, §14 execution contract) and refuses to act on assumption — when in doubt, the skill asks via `AskUserQuestion` and (for architectural choices) records the answer as an ADR in `Docs/Decisions/NNNN-titulo.md`.

### Orchestrators (3)

| Skill | Role |
|---|---|
| `vx-phase-analyze` | Detailed formato-2 plan for one MVP fase (cross-task contract registry + canonical commands). Output feeds `vx-task-create`. |
| `vx-task-create` | Compiles a self-contained formato-2 task doc: consults specialists at CREATION time, bakes contracts/tests/commands, runs a cold-executor audit. |
| `vx-task-execute` | Mechanical execution of a formato-2 task: format gate → read budget → deps → pre-flight → tdd → exact gate → commit/push → PR. No specialist consultation at runtime. |

### Specialists (12)

| Skill | Domain |
|---|---|
| `vx-spec-architecture` | Module boundaries, Public/Private, multi-backend slots |
| `vx-spec-rendering` | D3D12 RHI, RenderGraph, GBuffer, deferred + Forward+, CSM, IBL, GTAO, SSR, TAA, FSR |
| `vx-spec-shaders` | HLSL SM 6.6+, DXC, DXIL cache, permutations, hot reload |
| `vx-spec-physics` | Jolt, CharacterVirtual, layers, raycast/sweep, ragdoll |
| `vx-spec-audio` | miniaudio, spatial 3D, buses, footsteps-by-surface, music crossfade |
| `vx-spec-animation` | Skeleton, clips, blend tree, state machine, root motion, notifies, two-bone IK |
| `vx-spec-gameplay` | Character, combat, AI state machine, lock-on, camera, gameplay SDK |
| `vx-spec-tools` | Editor (Dear ImGui), AssetImporter CLI, manual TypeRegistry |
| **`vx-spec-memory-perf`** | **CRITICAL.** Allocators, D3D12MA, frame budget, Tracy/PIX, cache discipline. Consulted by anything per-frame. |
| `vx-spec-testing` | Catch2 suite, smoke, 100%-functional coverage, determinism |
| `vx-spec-ui-ux` | Game HUD in RmlUi (.rml/.rcss, data bindings — ADR 0010) + editor UX in Dear ImGui/ImGuizmo |
| `vx-spec-workflow` | **Socratic.** Designs operational pipelines (character setup, attack authoring, etc.); always asks the user; records ADRs. |

### Support (5)

| Skill | Role |
|---|---|
| `vx-hardening-guard` | Re-reads HARDENING.md and validates the proposed action against every rule. Returns OK / VIOLATION / AMBIGUOUS. Called by every other skill as terminal check. |
| `vx-naming-style` | Validates PascalCase, `m_PascalCase`, `V` prefix, `WorldStream` rename, Public/Private layout, `vx-*` folder names. |
| `vx-dependency-check` | Reads task `dependencies:`; BLOCKS execution if any dep is not `Implementado`. |
| `vx-git-flow` | Branch (`task/NN-nome`), commit (`[task NN]`), push, PR with exhaustive PT-BR body. Never `--no-verify`, `--force`, or `--amend` on shared commits. |
| `vx-doc-graph` | Maintains `Docs/Roadmap/README.md` index and Mermaid dependency graph; supports "show subtree for resource X" queries. |

### Skills auxiliares (Matt Pocock) — só via vx-*

Quatro skills externas estão integradas como sub-passos dos orquestradores, regidas por [Docs/References/MATT-SKILLS-BINDING.md](Docs/References/MATT-SKILLS-BINDING.md):

| Skill | Acionada por | Papel |
|---|---|---|
| `tdd` | `vx-task-execute` (passo 8) | Dirige o loop red-green-refactor a partir da tabela `## Plano de testes` (lista RED) assada no doc da task |
| `diagnose` | `vx-task-execute` (falha do gate) | Loop disciplinado de diagnóstico antes de "STOP/ask" |
| `grill-with-docs` | `vx-task-create`, `vx-spec-workflow` | Afia termos de domínio; mantém `Docs/Context/GLOSSARY.md` |
| `to-issues` | `vx-phase-analyze` | Disciplina de slices verticais para esboçar tasks (sem tracker externo) |

As demais skills do Matt (`triage`, `to-prd`, `prototype`, `improve-codebase-architecture`, `zoom-out`, `setup-matt-pocock-skills`) **não** estão integradas — ver o binding doc. A suíte caveman é tooling de comunicação (compressão de tokens), de uso avulso.

### Roadmap structure (created on first skill use, not pre-seeded)

```
Docs/
    Hardening/HARDENING.md       (already exists — master rules)
    design-mvp.md                (already exists — MVP contract)
    Roadmap/
        README.md                (index + dependency graph; created by vx-doc-graph)
        Phases/Fase-NN-name.md   (created by vx-phase-analyze)
        Tasks/NN-name.md         (created by vx-task-create)
        Backlog.md               (created when first item is deferred)
    Decisions/NNNN-titulo.md     (ADRs, created when an architectural choice is made)
```
