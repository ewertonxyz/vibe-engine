---
id: 13
nome: engine-core-bootstrap
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [03, 05, 06, 07, 08, 09]
decisions: [0006, 0007, 0012, 0021]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: false
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0006-bootstrap-centralizado.md
  - Docs/Decisions/0021-enginecore-bootstrap-decisoes.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md          # §Contratos entre tasks (T13), §Comandos canônicos
  - Docs/Roadmap/Tasks/03-core-logging-time-profile.md # apenas §Contratos (InitializeLogging, VLOG, SetLogCaptureForTesting)
  - Docs/Roadmap/Tasks/05-memory-tag-allocators.md     # apenas §Contratos (LinearAllocator ctor, IAllocator, MemoryTag)
  - Docs/Roadmap/Tasks/06-memory-frame-tracking.md     # apenas §Contratos (FrameAllocator, TrackingAllocator)
  - Docs/Roadmap/Tasks/09-jobsystem-worker-queue.md    # apenas §Contratos (JobSystem::Initialize/Shutdown/WorkerCount)
files_create:
  - Engine/Source/Runtime/EngineCore/CMakeLists.txt
  - Engine/Source/Runtime/EngineCore/Public/EngineCore/EngineCore.h
  - Engine/Source/Runtime/EngineCore/Private/EngineCore.cpp
  - Engine/Source/Runtime/EngineCore/Tests/EngineCore_Bootstrap.cpp
files_modify:
  - Engine/Source/Runtime/CMakeLists.txt
  - Engine/Source/Tests/CMakeLists.txt
---

# 13 — EngineCore: Initialize/Shutdown com ordem fixa

## Objetivo
Orquestrador central do runtime da Fase 1 (design-mvp.md §9; ADR 0006, ADR 0012): `EngineCore::Initialize`
sobe os subsistemas em ordem fixa (Logging→Memory→Platform→JobSystem→FileSystem) e `Shutdown` os derruba
em ordem inversa, com checagem de HW (≥ 8 núcleos físicos — ADR 0007) e relatório de leaks. Módulo próprio
`VibeEngineCore` (ADR 0012). O executor NÃO precisa ler o design — os contratos abaixo são completos.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso**
(HARDENING §14). T06/T07/T08/T09 ainda não estão implementadas na criação → o pre-flight compara os contratos
contra os `§Contratos` upstream; ao executar, o gate de dependências garante os headers reais.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4, §5, §7, §12, §14 |
| Decisions/0006 | ordem fixa de Initialize/Shutdown (5 passos + reverso) | inteiro |
| Decisions/0021 | forma `class EngineCore`, topologia de allocators, gate de leak por log, ordem observável, regra de HW | inteiro |
| Phases/Fase-01 | contrato T13 + comandos; assinaturas T07/T08 (GetPhysicalCoreCount, PlatformApplication) | §Contratos entre tasks (T07, T08, T13), §Comandos canônicos |
| Tasks/03 | `InitializeLogging`/`ShutdownLogging`/`VLOG_INFO`/`SetLogCaptureForTesting`/`LogLevel` | §Contratos |
| Tasks/05 | `LinearAllocator(Vuint64, MemoryTag=Core)`, `IAllocator`, `MemoryTag::Core` | §Contratos |
| Tasks/06 | `FrameAllocator`/`TrackingAllocator` (`ReportLeaks`/`LiveAllocationCount`) | §Contratos |
| Tasks/09 | `JobSystem::Initialize/Shutdown/IsInitialized/WorkerCount` | §Contratos |

## Escopo

### Dentro
- Alvo `Vibe::EngineCore` (STATIC) com 1 header Public, 1 TU Private, 1 arquivo de teste (7 testes).
- `EngineCore::Initialize/Shutdown` em ordem fixa (ADR 0006); resolução de `WorkerCount` (ADR 0007); rollback em falha.
- Estado privado owned: system `LinearAllocator` + `TrackingAllocator` (opt-in) + `FrameAllocator` por worker (ADR 0021).
- Cada passo emite `VLOG_INFO` marcador; `Shutdown` loga `"TrackingAllocator: <N> leaks"` (ADR 0021).

### Fora (NÃO fazer, mesmo que pareça óbvio)
- O smoke global `[smoke][fase1]` e `Scripts/run-tests.ps1` (T14).
- Linkar `Vibe::FileSystem` (stateless na Fase 1 — o passo FileSystem é só marcador de log; ADR 0021).
- `VPROFILE_ZONE` em Initialize/Shutdown (boot não é per-frame — §13). Janela/loop de jogo (Fase 12).
- Expor accessor de allocator ou de leak-count (registro mínimo — ADR 0021).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_modify`: raiz do Runtime (`add_subdirectory(EngineCore)`) + `Engine/Source/Tests/CMakeLists.txt`
(linkar `Vibe::EngineCore` em `VibeTests`; fontes auto-globadas — ADR 0016). Exceções permanentes: este doc,
README, Backlog. Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE quando: (a) contrato não compila vs `<Core/...>`/`<Memory/...>`/`<Platform/...>`/`<JobSystem/...>` reais;
(b) precisar de arquivo fora das listas (ex.: `PlatformThread::GetPhysicalCoreCount` não existir → drift da T07);
(c) seções se contradizem; (d) teste impossível; (e) gate falha após 2 ciclos de `diagnose`. Relate: **passo ·
trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <EngineCore/EngineCore.h>`. Doxygen §5 completo.)

### Public/EngineCore/EngineCore.h
```cpp
#include <Core/Result.h>   // VResult
#include <Core/Types.h>    // Vuint32, Vuint64

namespace VibeEngine
{
/// @brief Falhas de bootstrap do runtime da Fase 1 (ADR 0021).
enum class EngineError : Vuint32
{
    Unknown,             ///< @brief Falha não categorizada (default defensivo).
    AlreadyInitialized,  ///< @brief Initialize chamado com o runtime já vivo.
    InsufficientHardware,///< @brief < 8 núcleos físicos em modo auto (ADR 0007).
    SubsystemInitFailed  ///< @brief Um passo do boot (ADR 0006) falhou ao subir.
};

/// @brief Configuração de bootstrap. Valores 0 acionam defaults; m_EnableTracking ignorado em Shipping.
struct EngineCoreConfig
{
    Vuint32 m_WorkerCount { 0 };        ///< @brief Workers do JobSystem; 0 = auto = max(8, físicos-1) (ADR 0007).
    Vuint64 m_FrameAllocatorSize { 0 }; ///< @brief Bytes por FrameAllocator por worker; 0 = default 1 MiB (ADR 0006).
    bool m_EnableTracking { true };     ///< @brief Liga o TrackingAllocator (ignorado em Shipping — ADR 0003).
};

/**
 * @brief Orquestrador estático do runtime da Fase 1 (ADR 0012); sobe/derruba subsistemas em ordem fixa.
 * @details Leaf inverso do Core: depende de Core+Memory+Platform+JobSystem (ADR 0006/0021). Não-instanciável.
 */
class EngineCore
{
public:
    EngineCore() = delete;

    /// @brief Sobe os subsistemas na ordem da ADR 0006. Idempotente-falho (já vivo → AlreadyInitialized).
    /// @param Config Parâmetros de bootstrap. @return ok; ou EngineError (rollback reverso em falha parcial).
    static VResult<void, EngineError> Initialize(const EngineCoreConfig& Config);
    /// @brief Derruba os subsistemas na ordem inversa (ADR 0006); no-op seguro se não inicializado.
    static void Shutdown();
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **Ordem de Initialize** (ADR 0006), cada passo com `VLOG_INFO` marcador (string canônica `"EngineCore: <Passo>"`
  com Passo ∈ {`Logging`,`Memory`,`Platform`,`JobSystem`,`FileSystem`}): 1. `InitializeLogging()` (T03). 2. Memory:
  constrói o system `LinearAllocator(SystemBytes=1 MiB, MemoryTag::Core)` e, se `m_EnableTracking`, o
  `TrackingAllocator(systemAllocator)`. 3. Platform: `PlatformApplication::Initialize()` (T08); resolve
  `WorkerCount` (ver abaixo). 4. `JobSystem::Initialize(ResolvedWorkerCount)` (T09); depois constrói um
  `FrameAllocator(FrameBytes)` por worker (`WorkerCount()` deles; `FrameBytes = m_FrameAllocatorSize ? : 1 MiB`).
  5. FileSystem: **no-op** (stateless — só o marcador de log; ADR 0021).
- **Ordem de Shutdown** (inversa), também com `VLOG_INFO` marcador: FileSystem (no-op) → `JobSystem::Shutdown()`
  (join workers) → `PlatformApplication::Shutdown()` → **Memory**: destrói os `FrameAllocator`; se tracking,
  chama `TrackingAllocator::ReportLeaks()` e emite `VLOG_INFO` **`"TrackingAllocator: <N> leaks"`** (N de
  `LiveAllocationCount()`); destrói tracking+system allocator → `ShutdownLogging()` por ÚLTIMO (tudo acima pôde logar).
- **Resolução de WorkerCount** (ADR 0007): `m_WorkerCount==0` → `Auto = max(8u, PlatformThread::GetPhysicalCoreCount()-1)`;
  se `GetPhysicalCoreCount() < 8` em modo auto → rollback + `EngineError::InsufficientHardware`. `m_WorkerCount≥1`
  → usa o valor literal (override ignora o gate de HW). Assinatura upstream (T07, ADR 0021):
  `static Vuint32 VibeEngine::PlatformThread::GetPhysicalCoreCount() noexcept;` (de `<Platform/PlatformThread.h>`).
- **Idempotência**: flag estática privada `g_Initialized` em `EngineCore.cpp`. `Initialize` já-vivo → `AlreadyInitialized`
  (sem mexer no estado). `Shutdown` sem Initialize / repetido → no-op. Re-Initialize após Shutdown é permitido.
  **Rollback**: se um passo falhar, derruba os passos já concluídos em ordem inversa antes de retornar o erro.
- **Estado owned** vive num `struct` TU-local (file-scope `static`/`unique_ptr`) em `EngineCore.cpp` — sem membros
  no header (registro mínimo, §3/§4). RAII; sem `new`/`delete` cru (allocators são os caminhos — §12); sem C-cast.
- **Upstream assinaturas usadas (baked)**: `void InitializeLogging(); void ShutdownLogging();` e
  `void SetLogCaptureForTesting(void(*)(LogLevel,const char*));` (`#if VIBE_TESTING`) de `<Core/Logging.h>` (T03);
  `PlatformApplication::Initialize()->VResult<void,PlatformError>` / `Shutdown()` de `<Platform/PlatformApplication.h>` (T08);
  `FrameAllocator(Vuint64 ChunkBytes)`, `TrackingAllocator(IAllocator&)`/`ReportLeaks()`/`LiveAllocationCount()`,
  `LinearAllocator(Vuint64,MemoryTag)` de `<Memory/...>` (T06); `JobSystem::Initialize/Shutdown/IsInitialized/WorkerCount` (T09).
- **CMake** (`EngineCore/CMakeLists.txt`): `add_library(VibeEngineCore STATIC Private/EngineCore.cpp)` + alias
  `Vibe::EngineCore`; `target_include_directories(... PUBLIC Public PRIVATE Private)`;
  `target_link_libraries(VibeEngineCore PUBLIC Vibe::Core Vibe::Memory Vibe::Platform Vibe::JobSystem)`. Raiz do
  Runtime ganha `add_subdirectory(EngineCore)`; `Engine/Source/Tests/CMakeLists.txt` ganha `Vibe::EngineCore` no link de `VibeTests`.

## Plano de testes (lista RED — ordem de execução do `tdd`)
Fixture `EngineCoreFixture`: dtor chama `EngineCore::Shutdown()` incondicional (idempotente) — nenhum caso vaza
estado global, mesmo com `REQUIRE` falho (HARDENING §7). Ordem observada via `SetLogCaptureForTesting` (T03).

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `EngineCore_Smoke_Task13` | `[enginecore][smoke]` | EngineCore_Bootstrap.cpp | `Initialize({m_WorkerCount=8})` ok; `JobSystem::IsInitialized()`; `JobSystem::WorkerCount()==8`; `Shutdown()`; `!IsInitialized()`; < 1 s |
| 2 | `EngineCore_Initialize_OrderFixed` | `[enginecore][integration]` | EngineCore_Bootstrap.cpp | captura via hook: a sequência de marcadores `"EngineCore: <Passo>"` == [Logging, Memory, Platform, JobSystem, FileSystem]; `IsInitialized()` após |
| 3 | `EngineCore_Shutdown_ReverseOrder` | `[enginecore][integration]` | EngineCore_Bootstrap.cpp | após Init+Shutdown: marcadores de teardown == [FileSystem, JobSystem, Platform, Memory] (Logging por último, não capturável); `JobSystem::IsInitialized()==false`; `"TrackingAllocator: 0 leaks"` capturado |
| 4 | `EngineCore_InitializeTwice_AlreadyInitialized` | `[enginecore][unit]` | EngineCore_Bootstrap.cpp | 2º `Initialize` → `!has_value()`, `error()==EngineError::AlreadyInitialized`; 1º init intacto |
| 5 | `EngineCore_ReinitializeAfterShutdown` | `[enginecore][unit]` | EngineCore_Bootstrap.cpp | `Initialize`→`Shutdown`→`Initialize` 2º ciclo ok; `JobSystem::IsInitialized()` |
| 6 | `EngineCore_ShutdownTwice_Safe` | `[enginecore][unit]` | EngineCore_Bootstrap.cpp | `Init`→`Shutdown`→`Shutdown` 2ª vez no-op seguro; `!IsInitialized()` |
| 7 | `EngineCore_ManualWorkerCount_BypassesHwCheck` | `[enginecore][unit]` | EngineCore_Bootstrap.cpp | `Initialize({m_WorkerCount=2})` ok independente de núcleos; `WorkerCount()==2`; `EngineError::InsufficientHardware` referenciado (enum compila — ramo auto-<8 não forçável) |

**Smoke**: teste #1, < 30 s. **Determinismo** (HARDENING §7): sem wall-clock/RNG/sleep; ordem observada por
captura de log (não por timing); `WorkerCount=8/2` explícito evita o gate de HW (ADR 0007). **Cobertura**: cada
símbolo público (`EngineCoreConfig` campos, `EngineError` `AlreadyInitialized`/`InsufficientHardware`,
`Initialize`, `Shutdown`) aparece em ≥ 1 linha. **risco_memoria/frame false** → sem gate asan extra nem `[perf]`;
o gate de zero-leak é a linha `"TrackingAllocator: 0 leaks"` (teste #3). **Ramo auto-<8** não testado (não se força
< 8 núcleos em runtime — ADR 0021).

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos)

```powershell
# Configurar + build (gate: /W4 /WX falham com warning novo)
cmake --preset debug; cmake --preset development
cmake --build --preset debug; cmake --build --preset development
# Testes da task (lista RED)
ctest --preset debug --output-on-failure -R "EngineCore_"
# Smoke da task (com medição de duração)
Build\debug\bin\VibeTests.exe "[enginecore][smoke]" --durations yes
# Suite completa (gate final)
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
```

## Critério de aceitação
- [ ] 7 TEST_CASE do plano verdes — `ctest --preset debug -R "EngineCore_"`
- [ ] Smoke `EngineCore_Smoke_Task13` < 30 s (alvo < 1 s) — `--durations yes`
- [ ] Suite completa verde em debug e development (não regrediu)
- [ ] Ordem de Initialize/Shutdown documentada na ADR 0006 verificada pelos testes #2/#3
- [ ] `"TrackingAllocator: 0 leaks"` emitido no Shutdown e capturado (teste #3) — gate de zero-leak (§12)
- [ ] Header Public sem include de Private/, `<windows.h>`, spdlog; sem membros no header (estado em Private) — inspeção
- [ ] Sem `new`/`malloc` cru (allocators da engine); sem C-cast; sem `VPROFILE_ZONE` em boot (§13) — inspeção
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T13), ADR 0021. **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `EngineCore` (`Initialize`/`Shutdown`), `EngineCoreConfig`, `EngineError` | T14 (smoke da fase); fases 2+ (main de VibeGame/Editor, Fase 12) |
| Linha de log `"TrackingAllocator: <N> leaks"` (contrato com T14) | T14 (gate de zero-leak) |

## Hardening aplicável (referência + concretização)
- §3/§4/§5 → aqui: `EngineCore`/`EngineCoreConfig`/`EngineError` PascalCase, `m_*`; header em `Public/EngineCore/`; estado pesado só no `.cpp`; Doxygen completo.
- §7 → aqui: a tabela do plano É a aplicação; fixture com Shutdown no dtor; ordem por captura de log, não timing.
- §12 → aqui: allocators owned por RAII; sem `new` cru; gate de zero-leak via `ReportLeaks` logado.
- §13 → aqui: boot não é per-frame → nenhum `VPROFILE_ZONE`; `risco_frame:false`, sem `[perf]`.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
Formato → orçamento → deps (T03/T05/T06/T07/T08/T09 = Implementado) → pre-flight (`InitializeLogging`/`VLOG`/hook
vs Tasks/03; `LinearAllocator`/`IAllocator`/`MemoryTag` vs Tasks/05; `FrameAllocator`/`TrackingAllocator` vs
Tasks/06; `GetPhysicalCoreCount` + `PlatformApplication` vs Fase-01 §T07/§T08; `JobSystem` vs Tasks/09) →
Em-execução → baseline → branch `task/13-engine-core-bootstrap` → `tdd` sobre a lista RED → gate
completo → Implementado → commit `[task 13] enginecore: initialize/shutdown ordem fixa`, push, PR.

## Desvios aprovados
(vazio na criação)

## Bloqueio
(vazio)
## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §9 · Phases/Fase-01-foundation.md · Decisions/0006, 0007, 0012, 0021
