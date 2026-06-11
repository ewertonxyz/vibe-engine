---
id: 0015
titulo: CMAKE_BUILD_TYPE por preset + neutralização de flags default do MSVC
data: 2026-06-11
status: Aceita
relacionada: [Decisions/0008, Tasks/01-cmake-vcpkg-presets, Tasks/02-core-types-handles-result]
---

## Contexto

A Task 02 é o primeiro alvo que compila e linka dependências do vcpkg (Catch2). Ao buildar
o preset `development`, o linker falhou:

```
Catch2d.lib(...obj) : error LNK2038: mismatch detected for 'RuntimeLibrary':
  value 'MDd_DynamicDebug' doesn't match value 'MD_DynamicRelease' in Core_Smoke.cpp.obj
Catch2d.lib(...obj) : error LNK2038: mismatch detected for '_ITERATOR_DEBUG_LEVEL':
  value '2' doesn't match value '0' in Core_Smoke.cpp.obj
```

Duas causas, ambas originadas no desenho dos presets da Task 01 (ADR 0008), que só ficaram
visíveis quando algo realmente compilou:

1. **`CMAKE_BUILD_TYPE` indefinido nos presets.** A toolchain força `CMAKE_BUILD_TYPE=Debug`
   em todos os presets. Com isso o MSVC injeta `CMAKE_CXX_FLAGS_DEBUG` (`/Od /Ob0 /RTC1`) e o
   runtime de debug (`/MDd`) **depois** das flags do preset (`CMAKE_CXX_FLAGS` da ADR 0008).
   Em `debug` as flags coincidem (mascarado); em `development` sobrescrevem `/O2 /MD` → warning
   `D9025` e binário de development compilado como debug.
2. **vcpkg seleciona a variante da dependência por `CMAKE_BUILD_TYPE`.** Com `Debug` forçado, o
   vcpkg linka `Catch2d.lib` (debug, `/MDd`, `_ITERATOR_DEBUG_LEVEL=2`) mesmo no preset
   `development` (`/MD`) → LNK2038.

A ADR 0008 deixa claro que as flags por preset são a única fonte de verdade de otimização e
runtime; o comportamento acima viola essa intenção.

## Opções consideradas

- **A. `CMAKE_BUILD_TYPE` por preset** em `CMakePresets.json` (debug/asan=Debug,
  development/shipping=Release) + neutralizar os flags default por configuração no
  `CMakeLists.txt` raiz. vcpkg passa a linkar a variante correta; flags da ADR 0008 voltam a
  ser autoritativas.
- **B. Workaround no `CMakeLists.txt` raiz da Task 02**: manter `CMAKE_BUILD_TYPE=Debug` forçado
  e mapear `CMAKE_MAP_IMPORTED_CONFIG_DEBUG=Release` para development/shipping via inspeção de
  `VIBE_DEFINES`. Auto-contido na Task 02, mas compensa o gap em vez de corrigi-lo na origem.

## Decisão

**Opção A.** Cada preset declara `CMAKE_BUILD_TYPE` explícito:

| Preset | CMAKE_BUILD_TYPE | Variante vcpkg | Runtime (ADR 0008) |
|---|---|---|---|
| `debug` | `Debug` | debug | `/MDd` |
| `development` | `Release` | release | `/MD` |
| `shipping` | `Release` | release | `/MD` |
| `asan-debug` | `Debug` | debug | `/MDd` |

E o `CMakeLists.txt` raiz neutraliza os defaults por configuração do MSVC, de modo que as flags
do preset (ADR 0008) sejam a única fonte:

```cmake
foreach(VibeCfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_CXX_FLAGS_${VibeCfg} "" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_${VibeCfg} "" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_${VibeCfg} "" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_${VibeCfg} "" CACHE STRING "" FORCE)
endforeach()
set(CMAKE_MSVC_RUNTIME_LIBRARY "")
```

A escolha foi do usuário (execução da Task 02), preferindo corrigir a origem em vez do
workaround. Como a Task 01 já está `Implementado`, a edição de `CMakePresets.json` foi
autorizada explicitamente e registrada como Desvio aprovado na Task 02.

## Consequências

- vcpkg passa a linkar a variante (debug/release) coerente com o runtime de cada preset; sem
  mais LNK2038 de `RuntimeLibrary` / `_ITERATOR_DEBUG_LEVEL`.
- As flags da ADR 0008 são efetivamente as únicas aplicadas (sem `/Od`/`/DNDEBUG`/runtime
  automático do MSVC contaminando). `NDEBUG`, se desejado em shipping, deve ser adicionado
  explicitamente à tabela da ADR 0008 (não é introduzido aqui).
- `CMakePresets.json` (Task 01) ganha `CMAKE_BUILD_TYPE` por preset — extensão da ADR 0008, sem
  alterar nomes de preset nem o contrato de Interface da T01 (presets, `Build/<preset>/bin`,
  defines `VIBE_*`).
- Tasks futuras que adicionarem preset seguem esta tabela.

## Adendo (Task 03) — anotações de container do ASan

Ao buildar o preset `asan-debug` pela primeira vez com um alvo que linka libs do vcpkg
(`VibeTests` → `Catch2d.lib`), surgiu LNK2038: `mismatch detected for 'annotate_vector'/'annotate_string'`
(valor `1` nos objs compilados com `/fsanitize=address` vs `0` nas libs vcpkg sem ASan). As libs do
vcpkg não são compiladas com ASan, então as anotações de overflow de container são incompatíveis.

Decisão (usuário, execução da Task 03): adicionar `_DISABLE_STL_ANNOTATION=1` aos defines do preset
`asan-debug` em `CMakePresets.json`. Esse é o macro mestre da STL do MSVC
(`__msvc_sanitizer_annotate_container.hpp`) que desliga as anotações de `basic_string`, `vector` **e**
`optional` de uma vez (os macros granulares `_DISABLE_STRING_ANNOTATION`/`_DISABLE_VECTOR_ANNOTATION`
deixavam `annotate_optional` escapar). Isso casa o ABI dos containers com as libs não-ASan e
desbloqueia o `asan-debug` para qualquer alvo que linke dependências do vcpkg. Edição de
`CMakePresets.json` (T01, `Implementado`) autorizada explicitamente.
