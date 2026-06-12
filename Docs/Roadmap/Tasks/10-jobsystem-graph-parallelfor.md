---
id: 10
nome: jobsystem-graph-parallelfor
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [05, 09]
decisions: [0002, 0004, 0014, 0018, 0019]
especialistas_consultados: [vx-spec-memory-perf, vx-spec-architecture, vx-spec-testing]
risco_memoria: true
risco_frame: true
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Decisions/0018-jobsystem-api-ciclo-de-vida.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md          # §Contratos entre tasks (T09/T10), §Comandos canônicos
  - Docs/Roadmap/Tasks/02-core-types-handles-result.md # apenas §Contratos (Vuint*/Vbyte/Vspan)
  - Docs/Roadmap/Tasks/03-core-logging-time-profile.md # apenas §Contratos (VPROFILE_ZONE)
  - Docs/Roadmap/Tasks/05-memory-tag-allocators.md     # apenas §Contratos (IAllocator, MemoryTag, LinearAllocator)
  - Docs/Roadmap/Tasks/09-jobsystem-worker-queue.md    # apenas §Contratos (JobSystem, JobFence, JobHandle, Schedule)
files_create:
  - Engine/Source/Runtime/JobSystem/Public/JobSystem/ParallelFor.h
  - Engine/Source/Runtime/JobSystem/Public/JobSystem/TaskGraph.h
  - Engine/Source/Runtime/JobSystem/Private/TaskGraph.cpp
  - Engine/Source/Runtime/JobSystem/Tests/Job_Smoke_Task10.cpp
  - Engine/Source/Runtime/JobSystem/Tests/Job_ParallelFor.cpp
  - Engine/Source/Runtime/JobSystem/Tests/Job_TaskGraph.cpp
files_modify:
  - Engine/Source/Runtime/JobSystem/CMakeLists.txt
---

# 10 — JobSystem: ParallelFor + TaskGraph

## Objetivo
Construtos de alto nível do `JobSystem` (design-mvp.md §8.5, §11): `ParallelFor` (laço paralelo por
intervalo, chunks estáticos) e `TaskGraph` (DAG single-shot de jobs com dependências, `Submit() -> JobFence`),
ambos sobre o facade estático `JobSystem` da T09 (ADR 0018). Armazenamento do grafo via `IAllocator&`
(`MemoryTag::Job`, T05). O executor NÃO precisa ler o design — os contratos abaixo são completos.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso**
(HARDENING §14). T05 e T09 ainda não estão implementadas na criação → o pre-flight compara os contratos
contra `Tasks/05 §Contratos` e `Tasks/09 §Contratos`; ao executar, o gate de dependências garante os headers reais.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4, §5, §7, §12, §13, §14 |
| Decisions/0014 | include `<JobSystem/Foo.h>` / `<Memory/Foo.h>` / `<Core/Foo.h>` | inteiro |
| Decisions/0018 | facade estático, ParallelFor por intervalo, TaskGraph single-shot via IAllocator, fronteira T09/T10 | inteiro |
| Phases/Fase-01 | contrato T09/T10 + comandos (asan) | §Contratos entre tasks, §Comandos canônicos |
| Tasks/02 | `Vuint32`/`Vbyte`/`Vspan` | §Contratos |
| Tasks/03 | `VPROFILE_ZONE` (no-op sem TRACY_ENABLE) | §Contratos |
| Tasks/05 | `IAllocator` (`Allocate`/`Free` em `Vspan<Vbyte>`, `DefaultAlignment`), `MemoryTag` (`Job`), `LinearAllocator` | §Contratos |
| Tasks/09 | `JobSystem` (`Schedule`×2/`WorkerCount`/`Wait`), `JobFence`, `JobHandle`, `Detail::JobThunk` | §Contratos |

## Escopo

### Dentro
- 2 headers Public em `Public/JobSystem/` (`ParallelFor.h` header-only template, `TaskGraph.h`), 1 TU Private, 3 arquivos de teste.
- `ParallelFor(Count, ChunkSize, Body)`: divide `[0,Count)` em chunks, agenda via `JobSystem::Schedule(_, fence)`, bloqueia.
- `TaskGraph(IAllocator&)`: `AddNode`/`AddDependency`/`Submit() -> JobFence`; nós/arestas via allocator (`MemoryTag::Job`).
- Adicionar o link `Vibe::Memory` ao alvo `VibeJobSystem` (ADR 0018) e a fonte `Private/TaskGraph.cpp`.
- Tracy: `VPROFILE_ZONE("JobSystem.ParallelFor")` (ADR 0004).

### Fora (NÃO fazer, mesmo que pareça óbvio)
- Alterar o facade/`JobQueue`/`WorkerThread` da T09. Detecção de núcleos (T13). Backend de fibers (§5.4).
- `std::function` como payload, captura > 48 B (mesma guarda da T09 — ADR 0002, §13).
- Reuso de `TaskGraph` após `Submit()` (single-shot — ADR 0018). Tocar `Engine/Source/Tests/CMakeLists.txt` (T09 já linkou `Vibe::JobSystem`; fontes auto-globadas — ADR 0016).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_modify` é SÓ `JobSystem/CMakeLists.txt` (adicionar `Private/TaskGraph.cpp` à lista de fontes e
`Vibe::Memory` ao link de `VibeJobSystem`). Exceções permanentes: este doc, README, Backlog. Outro arquivo →
Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) contrato não compila vs `<JobSystem/JobSystem.h>`/`<Memory/IAllocator.h>`
reais (drift da T09/T05); (b) precisar de arquivo fora das listas (ex.: `Vibe::JobSystem` não linkado em VibeTests →
drift da T09); (c) seções se contradizem; (d) teste impossível; (e) gate (incl. asan/perf) falha após 2 ciclos
de `diagnose`. Relate: **passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <JobSystem/Foo.h>`. Doxygen §5 completo.)

### Public/JobSystem/ParallelFor.h
```cpp
#include <Core/Types.h>            // Vuint32
#include <Core/Profile.h>          // VPROFILE_ZONE
#include <JobSystem/JobFence.h>
#include <JobSystem/JobSystem.h>

namespace VibeEngine
{
/**
 * @brief Divide [0,Count) em chunks estáticos, executa `Body(Begin,End)` no pool e bloqueia até o fim.
 * @details Header-only (ADR 0018): cada chunk captura `{&Body, Begin, End}` (≤ 48 B, trivialmente copiável)
 *          num job; `Body` vive na pilha do chamador, que bloqueia em `JobFence::Wait()`, então sobrevive aos
 *          jobs. Requer `JobSystem` inicializado.
 * @param Count Total de iterações. `Count == 0` → no-op (Body nunca chamado).
 * @param ChunkSize Iterações por job; `0` ⇒ divisão uniforme `ceil(Count / JobSystem::WorkerCount())`.
 * @param Body Invocável `void(Vuint32 Begin, Vuint32 End)` (intervalo semiaberto; §13 — amortiza chamada).
 */
template <typename Fn>
void ParallelFor(Vuint32 Count, Vuint32 ChunkSize, Fn&& Body) noexcept
{
    VPROFILE_ZONE("JobSystem.ParallelFor");
    if (Count == 0) { return; }
    const Vuint32 Workers = JobSystem::WorkerCount();
    const Vuint32 Chunk = (ChunkSize == 0) ? ((Count + Workers - 1) / (Workers == 0 ? 1 : Workers)) : ChunkSize;
    JobFence Fence;
    for (Vuint32 Begin = 0; Begin < Count; Begin += Chunk)
    {
        const Vuint32 End = (Begin + Chunk < Count) ? (Begin + Chunk) : Count;
        const Fn* BodyPtr = &Body;
        JobSystem::Schedule([BodyPtr, Begin, End]() noexcept { (*BodyPtr)(Begin, End); }, Fence);
    }
    Fence.Wait();
}
}
```

### Public/JobSystem/TaskGraph.h
```cpp
#include <Core/Types.h>            // Vuint32, Vbyte, Vspan
#include <JobSystem/JobFence.h>
#include <JobSystem/JobSystem.h>   // Detail::JobThunk, JobSystem
#include <Memory/IAllocator.h>     // IAllocator, MemoryTag

#include <type_traits>

namespace VibeEngine
{
/**
 * @brief DAG single-shot de jobs com dependências explícitas; `Submit()` agenda e retorna um `JobFence`.
 * @details Fase de build single-thread (fria); nós/arestas em `IAllocator&` (`MemoryTag::Job` — ADR 0018).
 *          Não-reutilizável após `Submit()`. Não-copiável/não-movível.
 */
class TaskGraph
{
public:
    using NodeId = Vuint32;   ///< @brief Identidade opaca de um nó (índice no armazenamento do grafo).

    /// @brief Constrói um grafo vazio. @param Allocator fonte de memória dos nós/arestas (não-dono; deve sobreviver ao grafo).
    explicit TaskGraph(IAllocator& Allocator) noexcept;
    ~TaskGraph();                                       ///< @brief Libera nós/arestas no allocator.
    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;
    TaskGraph(TaskGraph&&) = delete;
    TaskGraph& operator=(TaskGraph&&) = delete;

    /// @brief Registra um nó de trabalho. @param Work callable (mesma guarda de 48 B da T09). @return id do nó.
    template <typename Fn>
    NodeId AddNode(Fn&& Work) noexcept
    {
        using D = std::decay_t<Fn>;
        static_assert(sizeof(D) <= 48, "Captura de nó > 48 B (ADR 0002): aloque em FrameAllocator e passe um ponteiro.");
        static_assert(std::is_trivially_copyable_v<D>, "Payload de nó deve ser trivialmente copiável (ADR 0002, §13).");
        static_assert(std::is_trivially_destructible_v<D>, "Payload de nó deve ser trivialmente destrutível (ADR 0002).");
        D Local = static_cast<Fn&&>(Work);
        return AddNodeImpl(&Detail::JobThunk<D>, Vspan<const Vbyte>{ reinterpret_cast<const Vbyte*>(&Local), sizeof(D) });
    }

    /// @brief Declara que `After` só roda após `Before` completar. @param Before nó predecessor @param After nó dependente.
    void AddDependency(NodeId Before, NodeId After) noexcept;
    /// @brief Agenda todo o grafo respeitando as dependências; consome o grafo. @return fence de conclusão do grafo.
    [[nodiscard]] JobFence Submit() noexcept;

private:
    /// @brief Registra um nó type-erased. @param Entry trampolim @param Payload bytes (≤ 48) @return id do nó.
    NodeId AddNodeImpl(void (*Entry)(void*), Vspan<const Vbyte> Payload) noexcept;
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **`ParallelFor` é header-only** e bloqueia em `Fence.Wait()` (ADR 0018) — por isso pode capturar `&Body`:
  o chamador bloqueia, então `Body` sobrevive aos chunks. O lambda de chunk captura `{const Fn*, Vuint32, Vuint32}`
  = 16 B trivialmente copiável → passa a guarda de 48 B da T09. `Count==0` retorna antes de criar a fence.
  Não criar alocação por iteração (§13). `WorkerCount()==0` (não inicializado) é proibido — exige `JobSystem` ativo.
- **`TaskGraph` usa `Job` (T09, Private)** para os nós: cada nó guarda `{Entry, payload 48 B, m_PredecessorsCount}`.
  `AddDependency(Before, After)` incrementa `m_PredecessorsCount` de `After` e registra `After` na lista de
  sucessores de `Before` (campos do `Job`, ADR 0002). `Submit()`: agenda os nós com `m_PredecessorsCount==0`;
  ao completar um nó, decrementa os sucessores e agenda os que zeram — tudo via `JobSystem::Schedule(_, fence)`.
  `Submit()` retorna a `JobFence` cobrindo o grafo; após `Submit()` o grafo é inválido (single-shot).
- **Armazenamento via `IAllocator&`** (`MemoryTag::Job`): arrays de nós + arestas alocados por `Allocate`
  (alinhamento `IAllocator::DefaultAlignment`), liberados no destrutor com `Free` (RAII). Sem `new`/`malloc` cru;
  sem C-cast (`static_cast`/`reinterpret_cast`). Interfaces em `Vspan`, nunca ptr+size (§12).
- **`AddNode` mantém `Job` Private**: o template Public faz a guarda de 48 B e delega a `AddNodeImpl` (Private,
  em TaskGraph.cpp) recebendo `Vspan<const Vbyte>`, reusando `Detail::JobThunk` da T09.
- **CMake**: `JobSystem/CMakeLists.txt` (files_modify) ganha `Private/TaskGraph.cpp` na lista de fontes de
  `VibeJobSystem` **e** `Vibe::Memory` em `target_link_libraries(VibeJobSystem PUBLIC ...)` (ADR 0018). `ParallelFor.h`
  é header-only (sem `.cpp`). NADA mais (subdir e link de teste já feitos na T09).

## Plano de testes (lista RED — ordem de execução do `tdd`; fonte da política: ADR 0019)
Determinismo (§7): correção por invariante pós-`Fence::Wait`/`Submit().Wait()`; RNG só `std::mt19937{0xC0FFEEu}`;
reduções inteiras com `==` exato; sem `sleep_for`/wall-clock na correção. Wall-clock só no teste `[perf]`.
Cada teste faz seu próprio `JobSystem::Initialize`/`Shutdown` (sem pool global pré-existente).

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Job_Smoke_Task10` | `[job][smoke]` | Job_Smoke_Task10.cpp | `Initialize(8)`; `ParallelFor(N, 0, body)` soma paralela == soma serial; `TaskGraph` sobre um `LinearAllocator` diamante A→{B,C}→D `Submit().Wait()` todos rodam 1×; destrói o grafo sem leak (asan); `Shutdown()`; < 1 s |
| 2 | `Job_ParallelFor_SumEqualsSerial` | `[job][unit]` | Job_ParallelFor.cpp | array de N preenchido por `mt19937{0xC0FFEEu}`; `ParallelFor` reduz por chunk; soma paralela `==` serial (inteiro, exato) |
| 3 | `Job_ParallelFor_EachIndexWrittenOnce` | `[job][unit]` | Job_ParallelFor.cpp | `hits[i]` (atomic) incrementado em `[Begin,End)`; para ChunkSize∈{0,1,64,Count} cada `hits[i]==1` (cobertura disjunta, sem gap) |
| 4 | `Job_ParallelFor_EdgeCounts` | `[job][unit]` | Job_ParallelFor.cpp | `Count==0` → Body nunca chamado, sem crash; `Count==1`; `Count` não-divisível por ChunkSize; `ChunkSize>=Count` (1 chunk); união dos intervalos == `[0,Count)` sem overlap |
| 5 | `Job_ParallelFor_NoDataRace` | `[job][unit]` | Job_ParallelFor.cpp | escrita disjunta em `std::vector<Vint32>` (cada índice só o seu, sem atomic); cada slot == valor esperado (veredito de corrida vem do asan-debug — ADR 0019) |
| 6 | `Job_TaskGraph_RespectsDependencies` | `[job][integration]` | Job_TaskGraph.cpp | grafo (cadeia + diamante A→B, A→C, B→D, C→D) via `TaskGraph(alloc)`; cada nó anota seu `NodeId` num log sob a fence; `Submit().Wait()`; para cada aresta `Before→After`: `indexOf(Before) < indexOf(After)` |
| 7 | `Job_TaskGraph_FenceWaitCompletesAll` | `[job][unit]` | Job_TaskGraph.cpp | grafo de G nós cada `fetch_add(1)`; `JobFence f = Submit(); f.Wait();` contador==G sem sync extra; cada nó 1× |
| 8 | `Job_Perf_ParallelForThroughput` | `[job][perf]` | Job_ParallelFor.cpp | `ParallelFor` sobre K=1000000 (trabalho barato por elemento); resultado paralelo `==` serial (exato) **E** paralelo ≤ serial **E** wall-clock < `kPerfCeilingMs` (constexpr generoso provisório — calibrar por Tracy, ADR 0019); emite `VPROFILE_ZONE` |

**Smoke**: teste #1, < 30 s. **Gate de memória**: `risco_memoria: true` → **asan-debug** verde; o `TaskGraph`
usa `IAllocator` (um `LinearAllocator` no teste) → o asan-debug é o veredito de zero-leak/UAF ao destruir o
grafo (TrackingAllocator global é a T14 — precedente T05; §12). **Gate de frame**: `risco_frame: true` → teste #8 `[job][perf]`. **Cobertura**:
cada símbolo público dos contratos (`ParallelFor`; `TaskGraph` ctor/`AddNode`/`AddDependency`/`Submit`/`NodeId`)
aparece em ≥ 1 linha.

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos + gate asan)

```powershell
# Configurar (uma vez por preset)
cmake --preset debug
cmake --preset development

# Build (gate: /W4 /WX falham com warning novo)
cmake --build --preset debug
cmake --build --preset development

# Testes da task (lista RED, inclui [perf])
ctest --preset debug --output-on-failure -R "Job_"

# Smoke da task (com medição de duração)
Build\debug\bin\VibeTests.exe "[job][smoke]" --durations yes

# Suite completa (gate final)
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure

# Gate de memória (risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure -R "Job_"
```

## Critério de aceitação
- [ ] 8 TEST_CASE do plano verdes — `ctest --preset debug -R "Job_"` (inclui os 8 da T09; total `[job]` ≥ 16)
- [ ] Smoke `Job_Smoke_Task10` < 30 s (alvo < 1 s) — saída de `--durations yes`; grafo destruído sem leak (asan)
- [ ] Teste `[job][perf]` verde (correção exata + paralelo ≤ serial + teto generoso) — `ctest -R "Job_Perf_ParallelForThroughput"`
- [ ] Suite completa verde em debug e development (não regrediu; JobSystem da T09 intacto)
- [ ] **asan-debug verde** — `ctest --preset asan-debug -R "Job_"` (gate de memória; TaskGraph via IAllocator, zero-leak)
- [ ] `TaskGraph` sem `new`/`malloc` cru (storage via `IAllocator`); RAII no destrutor; interfaces em `Vspan` — inspeção
- [ ] Headers Public sem `<thread>`/`<windows.h>`/Private/; `ParallelFor.h` header-only — inspeção
- [ ] `VPROFILE_ZONE("JobSystem.ParallelFor")` presente — inspeção (ADR 0004)
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T09/T10), ADR 0018. **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `ParallelFor(Count, ChunkSize, Body)` (Body por intervalo) | T12, T13, T14 e fases 2+ (Renderer/visibility) |
| `TaskGraph` (`IAllocator&` ctor, `AddNode`/`AddDependency`/`Submit`, `NodeId`) | T12, T13 e fases 2+ |

## Hardening aplicável (referência + concretização)
- §3 → aqui: `ParallelFor`/`TaskGraph`/`NodeId` PascalCase; sem membros públicos crus.
- §4 → aqui: headers em `Public/JobSystem/`; `Job`/scheduling em Private (T09); include `<JobSystem/Foo.h>` (ADR 0014).
- §5 → aqui: Doxygen em toda declaração pública, inclusive `ParallelFor` e os templates.
- §7 → aqui: a tabela do plano É a aplicação; barreira = `Fence::Wait`/`Submit().Wait()`; RNG com seed fixa; `==` exato.
- §12 → aqui: `TaskGraph` via `IAllocator` (`MemoryTag::Job`), RAII, `Vspan`, sem C-cast; **gate asan-debug**.
- §13 → aqui: `ParallelFor` por intervalo (amortiza); sem alloc/`std::function` por iteração; Tracy; **teste `[perf]`**.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Dependências (T05 e T09 = Implementado) → 4. Pre-flight
(`JobSystem`/`JobFence`/`Detail::JobThunk` reais vs Tasks/09; `IAllocator`/`MemoryTag` vs Tasks/05) →
5. (git já bootstrapped) → 6. Status Em-execução → 7. Baseline (suite `[job]` da T09 verde) → 8. Branch
`task/10-jobsystem-graph-parallelfor` → 9. `tdd` sobre a lista RED → 10. Gate completo + asan-debug + perf →
11. Status Implementado → 12. Commit `[task 10] jobsystem: parallelfor + taskgraph`, push, PR.

## Desvios aprovados
(vazio na criação)

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §8.5, §11 · Phases/Fase-01-foundation.md · Decisions/0002, 0004, 0014, 0018, 0019
