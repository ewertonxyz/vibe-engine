---
id: 03
nome: core-logging-time-profile
fase: Fase-01-foundation
formato: 2
status: Implementado
dependencies: [02]
decisions: [0004, 0006, 0008, 0014]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: false
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0004-tracy-na-fase-1.md
  - Docs/Decisions/0006-bootstrap-centralizado.md
  - Docs/Decisions/0008-tooling-build-conventions.md   # tabela de defines por preset (TRACY_ENABLE, VIBE_BUILD_*)
  - Docs/Roadmap/Phases/Fase-01-foundation.md     # §Contratos entre tasks, §Comandos canônicos
  - Docs/Roadmap/Tasks/02-core-types-handles-result.md  # apenas §Contratos (Assert hook, Types)
files_create:
  - Engine/Source/Runtime/Core/Public/Core/Logging.h
  - Engine/Source/Runtime/Core/Public/Core/Time.h
  - Engine/Source/Runtime/Core/Public/Core/Profile.h
  - Engine/Source/Runtime/Core/Private/LoggingSink.cpp
  - Engine/Source/Runtime/Core/Private/TimeImpl.cpp
  - Engine/Source/Runtime/Core/Tests/Core_Logging.cpp
  - Engine/Source/Runtime/Core/Tests/Core_Time.cpp
files_modify:
  - Engine/Source/Runtime/Core/CMakeLists.txt
---

# 03 — Core: Logging, Time, Profile (Tracy macros)

## Objetivo
Completar a observabilidade do `Core`: `VLOG_*` sobre spdlog **síncrono** (ADR 0006), `HighResClock`/`FrameTimer` sobre `QueryPerformanceCounter`, e `VPROFILE_*` sobre Tracy (ADR 0004) — tudo no-op em Shipping. Conecta o assert da T02 ao log real via `Detail::SetAssertLogHook`. O executor NÃO precisa ler o design.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso** (HARDENING §14). Os headers reais da T02 (`Core/Types.h`, `Core/Assert.h`) existem após o gate de dependências — o pre-flight os compara com Tasks/02 §Contratos.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4 (include via ADR 0014), §5, §7, §13, §14 |
| Decisions/0004 | macros Tracy, no-op em Shipping, zonas iniciais | inteiro |
| Decisions/0006 | spdlog síncrono na Fase 1; Logging primeiro no boot | inteiro |
| Decisions/0008 | **defines por preset** (`TRACY_ENABLE` só em debug/development; `VIBE_BUILD_*`) | tabela "Flags e defines por preset" |
| Phases/Fase-01-foundation.md | contratos cross-task + comandos | §Contratos entre tasks, §Comandos canônicos |
| Tasks/02 | hook `Detail::SetAssertLogHook` + aliases de tipos | §Contratos |

## Escopo

### Dentro
- 3 headers Public (em `Public/Core/` — ADR 0014), 2 TUs Private, 6 testes, CMake do Core atualizado.
- Conectar `Detail::SetAssertLogHook` (declarado na T02) ao backend de log, dentro de `InitializeLogging()`.

### Fora (NÃO fazer)
- Log assíncrono (ADR futura, Fase 4). Overlay Tracy (Fase 3, ADR 0004).
- `FrameAllocator`/`MemoryTag` (T05/T06). `EngineCore::Initialize` (T13) — `InitializeLogging()` aqui é função pública chamável, não bootstrap global.
- Não modificar nenhum arquivo da T02. Nenhum `target_compile_definitions` para `VIBE_*`/`TRACY_ENABLE` (globais por preset — T01/ADR 0008).

### Semântica vinculante dos arquivos (HARDENING §14)
Listas exaustivas. Exceções permanentes: este doc, README do roadmap, Backlog. Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) header real da T02 difere do assumido (drift); (b) `<tracy/Tracy.hpp>` não resolve via vcpkg feature; (c) seções deste doc se contradizem; (d) teste impossível; (e) gate falha após 2 ciclos de `diagnose`. Relato: **passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <Core/Foo.h>` — ADR 0014)

### Engine/Source/Runtime/Core/Public/Core/Logging.h
```cpp
namespace VibeEngine
{
enum class LogLevel : Vuint8 { Info, Warn, Error };
void InitializeLogging();              // instala sinks + conecta Detail::SetAssertLogHook; idempotente
void ShutdownLogging();                // flush + desinstala; idempotente
void SetLogLevel(LogLevel Minimum);
namespace Core::Detail { void LogImpl(LogLevel Level, const char* Message); }
}
// Mensagem é um const char* já formado; SEM formatação printf/fmt na macro (formatação
// fica a cargo do chamador no MVP — decisão desta criação, mantém o header sem deps).
#define VLOG_INFO(Msg)   /* → Core::Detail::LogImpl(Info, Msg);  ((void)0) em Shipping */
#define VLOG_WARN(Msg)   /* idem, Warn */
#define VLOG_ERROR(Msg)  /* idem, Error — também ((void)0) em Shipping (ADR 0006/0008) */
```

### Engine/Source/Runtime/Core/Public/Core/Time.h
```cpp
namespace VibeEngine
{
class HighResClock
{
public:
    static Vuint64 NowTicks() noexcept;                    // QueryPerformanceCounter
    static Vdouble TicksToSeconds(Vuint64 Ticks) noexcept; // frequência cacheada
};
class FrameTimer
{
public:
    void Tick() noexcept;                       // atualiza delta e incrementa frame
    Vdouble DeltaSeconds() const noexcept;      // ≥ 0.0; 0.0 antes do 2º Tick
    Vuint64 FrameIndex() const noexcept;        // 0 antes do 1º Tick
private:
    Vuint64 m_LastTicks { 0 };
    Vdouble m_DeltaSeconds { 0.0 };
    Vuint64 m_FrameIndex { 0 };
};
}
```

### Engine/Source/Runtime/Core/Public/Core/Profile.h
```cpp
// Header macro-only. Inclui <tracy/Tracy.hpp> APENAS sob #ifdef TRACY_ENABLE.
#define VPROFILE_ZONE(Name)         /* ZoneScopedN(Name) com Tracy; ((void)0) sem */
#define VPROFILE_ZONE_COLORED(Name, Color)
#define VPROFILE_FRAME()            /* FrameMark */
#define VPROFILE_PLOT(Name, Value)  /* TracyPlot */
```

**Notas de implementação (decididas na criação; não re-derivar):**
- spdlog **síncrono** (ADR 0006); `LoggingSink.cpp` é o ÚNICO TU que inclui spdlog. `Logging.h` não expõe spdlog (a macro passa um `const char*` pronto a `Core::Detail::LogImpl`).
- TODOS os `VLOG_*` viram `((void)0)` quando `VIBE_BUILD_SHIPPING=1` — consistente com o perfil minimal de Shipping (ADR 0008) e o contrato da fase.
- `InitializeLogging()` chama `Core::Detail::SetAssertLogHook(&HookQueLogaComoErro)` (hook declarado na T02). Idempotente: segunda chamada é no-op; `ShutdownLogging()` 2× também.
- `TRACY_ENABLE` vem dos presets (tabela ADR 0008 — no contexto): definido em `debug`/`development`, ausente em `shipping`/`asan-debug`. Com ele ausente, `Profile.h` não referencia NENHUM símbolo Tracy (link limpo).
- `TimeImpl.cpp` é o ÚNICO TU com `<windows.h>` (`WIN32_LEAN_AND_MEAN`); `QueryPerformanceFrequency` cacheada em static local na primeira chamada.
- CMake: adicionar os novos sources a `VibeCore`; `find_package(Tracy CONFIG)` + link apenas quando o preset define `TRACY_ENABLE` (feature vcpkg `tracy` — ADR 0008); `find_package(spdlog CONFIG REQUIRED)` linkado PRIVATE.

## Plano de testes (lista RED — ordem de execução do `tdd`)

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Core_Smoke_Task03` | `[core][smoke]` | Core/Tests/Core_Logging.cpp | `InitializeLogging()` 2× (idempotente) → 1 `VLOG_INFO` capturado → `FrameTimer.Tick()` 2× com `DeltaSeconds() >= 0` → `VPROFILE_ZONE`/`VPROFILE_ZONE_COLORED`/`VPROFILE_FRAME`/`VPROFILE_PLOT` compilam e executam num bloco → `ShutdownLogging()` 2×; < 1 s |
| 2 | `Core_Logging_LevelFilter` | `[core][unit]` | Core/Tests/Core_Logging.cpp | `SetLogLevel(Error)` → `VLOG_INFO` não chega ao sink; `VLOG_ERROR` chega; restaurar nível no teardown |
| 3 | `Core_Logging_SinkReceivesFormatted` | `[core][unit]` | Core/Tests/Core_Logging.cpp | mensagem chega íntegra ao sink de captura (conteúdo exato) |
| 4 | `Core_Logging_AssertHookConnected` | `[core][unit]` | Core/Tests/Core_Logging.cpp | após `InitializeLogging()`, instalar handler de teste que delega ao hook → `VASSERT(false, "x")` gera linha no sink (prova que `SetAssertLogHook` foi conectado) |
| 5 | `Core_Time_HighResClockMonotonic` | `[core][unit]` | Core/Tests/Core_Time.cpp | duas leituras consecutivas: `t2 >= t1`; `TicksToSeconds(0)==0.0` |
| 6 | `Core_Time_FrameTimerDeltaNonNegative` | `[core][unit]` | Core/Tests/Core_Time.cpp | após 2 `Tick()`: `DeltaSeconds() >= 0.0` e `FrameIndex()==2`; sem comparação wall-clock (HARDENING §7) |

**Fixture `LogCapture`**: instala sink de captura no setup, remove no teardown — isolamento por teste (HARDENING §7). **Smoke**: teste #1. **Determinismo**: monotonia/contagem, nunca duração absoluta. **Cobertura**: cada símbolo público dos Contratos aparece em ≥ 1 linha acima (`SetAssertLogHook` da T02 é coberto aqui, no teste 4).

## Comandos (fonte: Fase-01 §Comandos canônicos + variantes por task com filtro `-R "Core_"` e tags `[core][smoke]`)

```powershell
cmake --preset debug
cmake --preset development
cmake --build --preset debug
cmake --build --preset development
ctest --preset debug --output-on-failure -R "Core_"
Build\debug\bin\VibeTests.exe "[core][smoke]" --durations yes
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
```

## Critério de aceitação
- [ ] 6 TEST_CASE do plano verdes — `ctest --preset debug -R "Core_"`
- [ ] Suite [core] inteira verde (T02 não regrediu) — mesmo comando
- [ ] Suite completa verde em debug e development
- [ ] `LoggingSink.cpp` é o único TU com spdlog; `TimeImpl.cpp` o único com `<windows.h>` — inspeção
- [ ] `Profile.h` não referencia Tracy sem `TRACY_ENABLE` — build do preset `asan-debug` configura e linka limpo
- [ ] Headers em `Public/Core/` e includes `<Core/Foo.h>` (ADR 0014) — inspeção
- [ ] Doxygen em toda declaração pública, incluindo macros (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T03). **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `VLOG_INFO/WARN/ERROR`, `InitializeLogging/ShutdownLogging`, `SetLogLevel` | T05–T14 (T13 inicializa primeiro — ADR 0006) |
| `HighResClock`, `FrameTimer` | T09, T13, T14 e fases 2+ |
| `VPROFILE_ZONE/ZONE_COLORED/FRAME/PLOT` | T09, T10, T12 (zonas obrigatórias — ADR 0004, HARDENING §13) |

## Hardening aplicável (referência + concretização)
- §3 → aqui: `VLOG_*`/`VPROFILE_*` SCREAMING_SNAKE com V; `m_LastTicks`/`m_DeltaSeconds`/`m_FrameIndex`.
- §4 → aqui: spdlog e Win32 confinados a Private/; Tracy só sob `#ifdef`; includes `<Core/Foo.h>` (ADR 0014).
- §5 → aqui: Doxygen com `@brief`/`@param` inclusive nas macros.
- §7 → aqui: fixture `LogCapture` isolada; tempo testado por monotonia.
- §13 → aqui: estas são as macros que TODO sistema per-frame futuro é obrigado a usar.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Dependências (T02 = Implementado) →
4. Pre-flight: `Core/Assert.h`/`Core/Types.h` reais vs Tasks/02 §Contratos → 5. (git já
bootstrapped) → 6. Status Em-execução → 7. Baseline (suite [core] da T02 verde) → 8. Branch
`task/03-core-logging-time-profile` → 9. `tdd` sobre a lista RED → 10. Gate completo →
11. Status Implementado → 12. Commit `[task 03] core: logging, time, profile`, push, PR.

## Desvios aprovados
- **LogCapture hook (aprovado pelo usuário via AskUserQuestion):** o plano de testes exige um fixture `LogCapture` que instala/remove sink de captura, mas spdlog é PRIVATE e o Contrato do `Logging.h` não expunha API de captura. Adicionado `void SetLogCaptureForTesting(void(*)(LogLevel,const char*))` guardado por `#if VIBE_TESTING` em `Logging.h` (precedente ADR 0009 / `VASSERT_SetHandlerForTesting`). Só em build de teste; nenhum símbolo de Interface para sucessoras mudou.
- **VibeTests por glob (ADR 0016, aprovado pelo usuário):** os testes novos precisavam entrar no alvo `VibeTests` (definido em `Engine/Source/Tests/CMakeLists.txt`, fora do `files_modify` da T03). A T02 implementara lista explícita, mas a nota da T02 dizia "`Core/Tests/*.cpp`" (glob). Correção na origem: `Engine/Source/Tests/CMakeLists.txt` convertido para `file(GLOB ... CONFIGURE_DEPENDS Engine/Source/Runtime/*/Tests/*.cpp)`. Adicionado também um POST_BUILD `copy_if_different $<TARGET_RUNTIME_DLLS:VibeTests>` (a cópia applocal do vcpkg não cobria `TracyClient.dll`) e `DISCOVERY_MODE PRE_TEST` no `catch_discover_tests` (descoberta no tempo do ctest, com as DLLs já no lugar). Arquivo fora do `files_modify` tocado **com autorização explícita** (correção da T02, `Implementado`) + nova ADR 0016. Nenhum símbolo de Interface mudou.
- **Profile.h `#if` em vez de `#ifdef` (inconsistência do doc):** o Contrato do `Profile.h` dizia incluir Tracy sob `#ifdef TRACY_ENABLE`, mas a ADR 0008 define `TRACY_ENABLE=0` em asan-debug (definido, valor 0) — `#ifdef` seria verdadeiro e tentaria incluir Tracy (não linkado em asan), violando o Critério "asan-debug linka limpo". Corrigido para `#if TRACY_ENABLE` (1→inclui, 0→pula, ausente→pula), consistente com o encoding de valor da ADR 0008 (e com `#if VIBE_BUILD_SHIPPING`). Mudança no próprio arquivo de `files_create`, sem alterar nome/comportamento de macro nem Interface.
- **asan-debug `_DISABLE_*_ANNOTATION` (ADR 0015 adendo, aprovado pelo usuário):** `asan-debug` falhava ao linkar `VibeTests` (LNK2038 `annotate_vector/string` 1 vs 0) por misturar objs ASan com `Catch2d.lib` do vcpkg sem ASan. Adicionado `_DISABLE_STL_ANNOTATION=1` (macro mestre da STL do MSVC: string+vector+optional) aos defines do preset `asan-debug` em `CMakePresets.json` (T01, `Implementado`) **com autorização explícita**. Documentado no adendo da ADR 0015.

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §4, §8.1 · Phases/Fase-01-foundation.md · Decisions/0004, 0006, 0008, 0014
