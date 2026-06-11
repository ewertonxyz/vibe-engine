---
id: 01
nome: cmake-vcpkg-presets
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: []
decisions: [0008]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: false
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0008-tooling-build-conventions.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md   # apenas §Contratos entre tasks (T01) e §Comandos canônicos
files_create:
  - CMakeLists.txt
  - CMakePresets.json
  - vcpkg.json
  - vcpkg-configuration.json
  - .gitignore
  - .editorconfig
  - .clang-format
  - Scripts/run-tests.ps1
files_modify: []
---

# 01 — Setup CMake + Ninja + vcpkg + 4 presets

## Objetivo
Bootstrap do tooling de build. Sem isso, nenhuma task posterior compila. Estabelece a reprodutibilidade exigida por design-mvp.md §3 #10 ("clean clone → build em ≤ 30 min") e as convenções da ADR 0008. O executor NÃO precisa ler o design — tudo que importa está neste doc.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este documento, (2) os arquivos de `contexto:`, (3) os arquivos que ele próprio criar. **Nada além disso** (HARDENING §14). Fato faltante → Protocolo de bloqueio.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras inegociáveis | §3, §6, §14 |
| Docs/Decisions/0008-tooling-build-conventions.md | flags, presets, clang-format, baseline — fonte dos contratos | inteiro |
| Docs/Roadmap/Phases/Fase-01-foundation.md | convenções T01 + comandos canônicos | §Contratos entre tasks (T01), §Comandos canônicos |

## Escopo

### Dentro
- Os 8 arquivos de `files_create`, com o conteúdo dos Contratos abaixo.
- Nenhum alvo de biblioteca/executável ainda (módulos entram via T02+).

### Fora (NÃO fazer, mesmo que pareça óbvio)
- Não criar `Engine/Source/**` nem alvo `VibeTests` (nasce na T02).
- Não adicionar dependências vcpkg além das listadas (rmlui, jolt etc. entram nas fases que as usam).
- Não configurar CI, pre-commit hooks ou Doxyfile.

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create` é exaustivo; criar qualquer outro arquivo é violação. Exceções permanentes: este doc, `Docs/Roadmap/README.md`, `Docs/Roadmap/Backlog.md`. Precisa de outro arquivo? → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) um comando dos Contratos falha de forma não-resolvível em 2 ciclos de `diagnose`; (b) precisar de arquivo fora da lista; (c) seções deste doc se contradizem. Relate: **passo · trecho do doc · erro literal · comando mínimo de reprodução · resolução sugerida**. Mudança de seção assada → rebake via `vx-task-create` + status `Bloqueado` + `## Bloqueio`.

## Contratos (conteúdo esperado por arquivo — task de tooling, sem C++)

### CMakePresets.json
- 4 configure presets: `debug`, `development`, `shipping`, `asan-debug` — flags e defines EXATAMENTE pela tabela da ADR 0008 (contexto).
- Todos: `generator: "Ninja"`, `binaryDir: "${sourceDir}/Build/${presetName}"`, `toolchainFile: "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"`.
- `debug`/`development`: `VCPKG_MANIFEST_FEATURES=tracy` (ADR 0008 §features).
- 4 build presets e 4 test presets espelhando os configure presets (`ctest --preset <nome>` funciona).

### CMakeLists.txt (raiz)
- `cmake_minimum_required(VERSION 3.28)`; `project(VibeEngine LANGUAGES CXX)`.
- Flags MSVC comuns (ADR 0008): `/std:c++latest /Zc:__cplusplus /Zc:preprocessor /permissive- /W4 /WX /MP /utf-8 /EHsc`.
- `set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)` — executáveis em `Build/<preset>/bin/` (contrato T01 na fase).
- `enable_testing()` + `find_package(Catch2 3 CONFIG REQUIRED)` (resolve via vcpkg; NÃO cria alvo).
- Sem `add_subdirectory` ainda — comentário marcando onde T02 adicionará.

### vcpkg.json
- `name: "vibe-engine"`, `version-string: "0.1.0"`.
- `dependencies`: `glm`, `spdlog`, `catch2` (deps da Fase 1 — stack da fase).
- `features.tracy.dependencies`: `tracy` com `"default-features": false` (ADR 0008).

### vcpkg-configuration.json
- `default-registry` git → `https://github.com/microsoft/vcpkg`, `baseline` pinned a SHA real (NÃO vazio). Obter o SHA atual: `git ls-remote https://github.com/microsoft/vcpkg HEAD` e usar o hash completo.

### .clang-format
- YAML EXATO da ADR 0008 (BasedOnStyle Microsoft, ColumnLimit 120, IndentWidth 4, PointerAlignment Left, Allman, etc. — copiar o bloco inteiro do contexto).

### .gitignore
- `Build/`, `Intermediate/`, `Saved/`, `.vs/`, `.cache/`, `*.user`, `CMakeUserPresets.json`.

### .editorconfig
- `root = true`; `[*]`: `charset = utf-8`, `indent_style = space`, `indent_size = 4`, `insert_final_newline = true`; `[*.{json,yml,yaml}]`: `indent_size = 2`.

### Scripts/run-tests.ps1
- Parâmetro `-Preset` (default `debug`); executa `ctest --preset $Preset --output-on-failure`; propaga exit code.

**Notas de implementação:**
- A tabela de flags/defines por preset vive na ADR 0008 — copiar literalmente, não recriar de memória.
- `asan-debug`: sem `/RTC1` (incompatível com ASan), sem `TRACY_ENABLE` (ADR 0008).
- Requisito de ambiente: `VCPKG_ROOT` definido. Ausente → Protocolo de bloqueio (instrução ao usuário, não workaround silencioso).

## Plano de testes (verificações — sem Catch2 nesta task; alvo de teste nasce na T02)

| # | Verificação | Comando | Evidência esperada |
|---|---|---|---|
| 1 | Smoke: configure+build debug | `cmake --preset debug` e `cmake --build --preset debug` | exit 0 ("no work to do" é válido) |
| 2 | Configure development | `cmake --preset development` | exit 0 |
| 3 | Configure shipping | `cmake --preset shipping` | exit 0 |
| 4 | Configure asan-debug | `cmake --preset asan-debug` | exit 0 |
| 5 | Catch2 resolve | log do configure debug | "Found Catch2" (ou equivalente) sem erro |
| 6 | clang-format válido | `clang-format --dry-run --Werror .editorconfig`* em arquivo C++ dummy temporário | exit 0, sem diff |
| 7 | Baseline pinned | inspeção de vcpkg-configuration.json | campo `baseline` = SHA de 40 hex |
| 8 | run-tests.ps1 executa | `Scripts/run-tests.ps1 -Preset debug` | exit 0 (zero testes ainda é válido) |

\* criar o dummy em diretório temporário do sistema, fora do repo, e apagar ao final (não entra em files_create).

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos)

```powershell
cmake --preset debug
cmake --preset development
cmake --preset shipping
cmake --preset asan-debug
cmake --build --preset debug
Scripts/run-tests.ps1 -Preset debug
```

## Critério de aceitação
- [ ] Verificações 1–8 do Plano de testes verdes (comandos acima)
- [ ] Flags por preset idênticas à tabela da ADR 0008 — inspeção do CMakePresets.json
- [ ] `CMAKE_RUNTIME_OUTPUT_DIRECTORY` aponta para `Build/<preset>/bin` — inspeção do CMakeLists.txt
- [ ] vx-naming-style → OK nos arquivos criados
- [ ] vx-hardening-guard → OK

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T01). **Mudar qualquer item = Protocolo de bloqueio.**

| Convenção | Consumido por |
|---|---|
| Presets `debug`/`development`/`shipping`/`asan-debug` | T02–T14 |
| Executáveis em `Build/<preset>/bin/` | T02 (VibeTests), T14 |
| `find_package(Catch2 3)` disponível | T02 |
| Defines `VIBE_*`/`TRACY_ENABLE` por preset (ADR 0008) | T02, T03, T06 |

## Hardening aplicável (referência + concretização)
- §3 nomenclatura → aqui: nomes de arquivo de tooling são os canônicos (CMakeLists.txt, CMakePresets.json); `Scripts/` PascalCase.
- §6 git/PR → aqui: PR descreve quais flags MSVC e por quê, citando ADR 0008.
- §14 formato 2 → aqui: orçamento de leitura = 3 arquivos de contexto; nada mais.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Gate de dependências (vazio → libera) →
4. (sem pre-flight de contratos C++) → 5. Bootstrap git → 6. Status Em-execução →
7. (sem baseline — primeiro commit do tooling) → 8. Branch `task/01-cmake-vcpkg-presets` →
9. Implementar os 8 arquivos pelos Contratos → 10. Verificações 1–8 + vx-naming-style +
vx-hardening-guard → 11. Status Implementado → 12. Commit `[task 01] setup CMake + vcpkg + 4 presets`, push, PR.

## Desvios aprovados
(vazio)

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §3 #10, §4, §7 · Phases/Fase-01-foundation.md · Decisions/0008
