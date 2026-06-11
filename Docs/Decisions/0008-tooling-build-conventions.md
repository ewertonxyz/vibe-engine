---
id: 0008
titulo: Convenções de tooling e flags de build por preset
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation, Tasks/01-cmake-vcpkg-presets, Decisions/0003, Decisions/0004]
---

## Contexto

A Task 01 (setup CMake + vcpkg + 4 presets) precisa fixar três conjuntos de convenções não cobertos por ADRs anteriores:

1. Estilo do `.clang-format` (cosmético, mas estabelece convenção de leitura).
2. Estratégia de pinning do baseline do vcpkg (afeta reprodutibilidade — critério §3 #10 do design: clean clone build em ≤ 30 min).
3. Flags MSVC e defines de preprocessador por preset (consequência das ADRs 0003 — TrackingAllocator condicional, 0004 — Tracy condicional).

Sem estas decisões registradas, cada task posterior poderia divergir ao adicionar flags ad-hoc.

## Opções consideradas

- **clang-format**: Microsoft / LLVM / Google / custom from scratch.
- **vcpkg baseline**: pin a commit SHA / latest sem baseline / overrides por port.
- **Flags por preset**: variações de `/O*`, `/Z*`, sanitizers, defines `VIBE_BUILD_*`, `TRACY_ENABLE`, `VIBE_TESTING`, `VIBE_TRACKING_ALLOC`, `VIBE_ASSERTS`.

## Decisão

### Clang-format

`BasedOnStyle: Microsoft` como ponto de partida, com 4 overrides explícitos para alinhar ao HARDENING §3:

```yaml
BasedOnStyle: Microsoft
ColumnLimit: 120
IndentWidth: 4
UseTab: Never
PointerAlignment: Left          # int* Foo, não int *Foo
ReferenceAlignment: Left
AlignAfterOpenBracket: Align
BinPackArguments: false
BinPackParameters: false
AllowShortFunctionsOnASingleLine: Empty
BreakBeforeBraces: Allman
AccessModifierOffset: -4
NamespaceIndentation: None
```

### Vcpkg baseline

Pin a commit SHA específico do `microsoft/vcpkg` em `vcpkg-configuration.json`. Atualização do baseline é PR explícito com nota no commit ("update vcpkg baseline to <date>"). Reprodutibilidade total entre máquinas e tempo.

```jsonc
// vcpkg-configuration.json
{
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "<SHA fixo>"
  }
}
```

### Flags MSVC comuns a todos os presets

```
/std:c++latest /Zc:__cplusplus /Zc:preprocessor /permissive- /W4 /WX /MP /utf-8 /EHsc
```

### Flags e defines por preset

| Preset | Otimização | Debug info | Sanitizer | Defines |
|---|---|---|---|---|
| `debug` | `/Od /RTC1 /MDd /JMC` | `/Zi` | — | `VIBE_BUILD_DEBUG=1`, `TRACY_ENABLE=1`, `VIBE_TESTING=1`, `VIBE_TRACKING_ALLOC=1`, `VIBE_ASSERTS=1`, `_ITERATOR_DEBUG_LEVEL=2` |
| `development` | `/O2 /Oi /Ot /Ob2 /MD` | `/Zi` + linker `/DEBUG:FULL` | — | `VIBE_BUILD_DEVELOPMENT=1`, `TRACY_ENABLE=1`, `VIBE_TESTING=1`, `VIBE_TRACKING_ALLOC=1`, `VIBE_ASSERTS=1` |
| `shipping` | `/O2 /Oi /Ot /Ob3 /GL /Gy /GR-` + linker `/LTCG /OPT:REF /OPT:ICF` | — | — | `VIBE_BUILD_SHIPPING=1`, `VIBE_TESTING=0`, `VIBE_TRACKING_ALLOC=0`, `VIBE_ASSERTS=0` (sem `TRACY_ENABLE`) |
| `asan-debug` | `/Od /MDd /JMC` (sem `/RTC1` — incompatível) | `/Zi` | `/fsanitize=address` (sem `/INCREMENTAL`) | iguais ao `debug`, **exceto `TRACY_ENABLE=0`** (ASan + Tracy = falsos positivos) |

Todos os presets:
- `generator: "Ninja"`
- `toolchainFile: "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"` (ou caminho explícito do submódulo se for adotado)
- `binaryDir: "${sourceDir}/Build/${presetName}"`

### Vcpkg features

`vcpkg.json` declara feature `tracy`, com `default-features` da dep `tracy` em `false`. Presets que querem Tracy passam `-DVCPKG_MANIFEST_FEATURES=tracy`. Garante que builds de Shipping não baixem nem compilem o cliente Tracy.

## Consequências

- Qualquer task que adicione flag MSVC nova precisa estendê-la para os 4 presets respeitando esta tabela (ou registrar nova ADR se a flag desviar dela).
- `target_compile_definitions` propaga os defines `VIBE_*` PUBLIC para todos os módulos da engine. Headers Public podem usar `#if VIBE_BUILD_SHIPPING` etc. sem precisar incluir nada.
- Pin do vcpkg baseline = mudança em `vcpkg-configuration.json` requer aprovação explícita em PR (sinaliza intent de atualização).
- `clang-format` rodado em pre-commit (não obrigatório no MVP — convenção, não enforcement automático na Fase 1).
- Em `asan-debug`, `TRACY_ENABLE=0` significa que profiling não convive com sanitizer — uso é sequencial, não simultâneo.
