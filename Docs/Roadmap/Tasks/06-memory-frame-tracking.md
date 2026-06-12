---
id: 06
nome: memory-frame-tracking
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [03, 05]
decisions: [0001, 0003, 0008, 0014]
especialistas_consultados: [vx-spec-memory-perf, vx-spec-architecture, vx-spec-testing]
risco_memoria: true
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0001-tipos-fundamentais-core.md
  - Docs/Decisions/0003-allocators-e-diagnostico.md
  - Docs/Decisions/0008-tooling-build-conventions.md   # define VIBE_TRACKING_ALLOC por preset
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md   # §Contratos entre tasks (T05/T06), §Comandos canônicos
  - Docs/Roadmap/Tasks/05-memory-tag-allocators.md  # apenas §Contratos (IAllocator/MemoryTag/LinearAllocator)
  - Docs/Roadmap/Tasks/03-core-logging-time-profile.md  # apenas §Contratos (VLOG_*, SetLogCaptureForTesting)
files_create:
  - Engine/Source/Runtime/Memory/Public/Memory/FrameAllocator.h
  - Engine/Source/Runtime/Memory/Public/Memory/TrackingAllocator.h
  - Engine/Source/Runtime/Memory/Private/FrameAllocator.cpp
  - Engine/Source/Runtime/Memory/Private/TrackingAllocator.cpp
  - Engine/Source/Runtime/Memory/Tests/Memory_Frame.cpp
  - Engine/Source/Runtime/Memory/Tests/Memory_Tracking.cpp
  - Engine/Source/Runtime/Memory/Tests/Memory_Smoke_Task06.cpp
files_modify:
  - Engine/Source/Runtime/Memory/CMakeLists.txt
---

# 06 — Memory: FrameAllocator + TrackingAllocator

## Objetivo
Segunda fatia do módulo `Memory` (design-mvp.md §8.4, ADR 0003): `FrameAllocator` (bump allocator
de thread único que limpa por frame, cresce em chunks com aviso logado) e `TrackingAllocator`
(wrapper de diagnóstico que rastreia alocações vivas por tag em Debug/Development). Ambos derivam de
`IAllocator` (T05). O executor NÃO precisa ler o design — os contratos abaixo são completos.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além
disso** (HARDENING §14). T05 ainda não está implementada quando esta task é criada → o pre-flight
compara os contratos contra `Tasks/05 §Contratos`; ao executar, o gate de dependências garante os
headers reais.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4, §5, §7, §12, §14 |
| Decisions/0001 | MemoryTag fechado | inteiro |
| Decisions/0003 | FrameAllocator per-thread + grow; TrackingAllocator Debug/Dev | inteiro |
| Decisions/0008 | `VIBE_TRACKING_ALLOC` por preset (debug/dev/asan=1, shipping=0) | tabela de defines |
| Decisions/0014 | include `<Memory/Foo.h>` / `<Core/Foo.h>` | inteiro |
| Phases/Fase-01 | contrato T05/T06 + comandos (asan) | §Contratos entre tasks, §Comandos canônicos |
| Tasks/05 | `IAllocator`/`MemoryTag`/`LinearAllocator` | §Contratos |
| Tasks/03 | `VLOG_WARN`/`VLOG_ERROR`; `SetLogCaptureForTesting` (`#if VIBE_TESTING`) | §Contratos |

## Escopo

### Dentro
- 2 headers Public em `Public/Memory/`, 2 TUs Private, 9 testes (smoke first).
- `FrameAllocator`: bump per-thread que POSSUI chunks; cresce encadeando chunks com `VLOG_WARN`; `BeginFrame()` rebobina.
- `TrackingAllocator`: wrapper de `IAllocator&`; rastreia `{tag, size, thread_id}` por bloco; relatório por tag; só com `VIBE_TRACKING_ALLOC`.

### Fora (NÃO fazer)
- Integração com `JobSystem`/`BeginFrame` broadcast (T09). Nenhum `thread_local`/singleton global no FrameAllocator.
- `callsite` no record do TrackingAllocator (adiado — ADR 0003 adendo, decisão da criação).
- `StackAllocator`/`PoolAllocator`. Override global de `new` (ADR 0003).
- `target_compile_definitions VIBE_*` (globais por preset — ADR 0008).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create`/`files_modify` exaustivos. `files_modify` é SÓ `Memory/CMakeLists.txt` — a T05 já
adicionou `add_subdirectory(Memory)` na raiz do Runtime e já linkou `Vibe::Memory` em `VibeTests`
(fontes auto-globadas, ADR 0016). Exceções permanentes: este doc, README, Backlog. Outro arquivo →
Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) contrato não compila vs headers reais da T05/T03; (b)
precisar de arquivo fora das listas (ex.: o glob de testes ou o link `Vibe::Memory` NÃO existir →
drift da T05); (c) seções se contradizem; (d) teste impossível; (e) gate (incl. asan) falha após 2
ciclos de `diagnose`. Relate: **passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <Memory/Foo.h>`)

### Engine/Source/Runtime/Memory/Public/Memory/FrameAllocator.h
```cpp
#include <Core/Types.h>        // Vuint32, Vuint64, Vbyte, Vspan
#include <Memory/IAllocator.h>
#include <Memory/MemoryTag.h>

namespace VibeEngine
{
/**
 * @brief Bump allocator de frame, thread único, que POSSUI chunks encadeados (ADR 0003).
 * @details Cada thread instancia o SEU (não é TLS/singleton; integração JobSystem é T09). Aloca por
 *          bump; se não couber, encadeia um novo chunk (sem invalidar spans já servidos) e emite UM
 *          `VLOG_WARN`. `BeginFrame()` rebobina todos os chunks para reuso (não desaloca). `Free` é
 *          no-op. Tag canônica: `MemoryTag::Frame`. Não thread-safe.
 */
class FrameAllocator final : public IAllocator
{
public:
    /// @brief Tamanho default de chunk por instância (1 MiB — ADR 0003).
    static constexpr Vuint64 DefaultChunkBytes = 1ull << 20;

    /**
     * @brief Constrói e adquire o primeiro chunk (RAII).
     * @param ChunkBytes Tamanho de cada chunk e do grow (> 0; default 1 MiB).
     */
    explicit FrameAllocator(Vuint64 ChunkBytes = DefaultChunkBytes);
    ~FrameAllocator() override;                                  ///< @brief Libera todos os chunks (RAII).

    FrameAllocator(const FrameAllocator&) = delete;             ///< @brief Não-copiável.
    FrameAllocator& operator=(const FrameAllocator&) = delete;
    FrameAllocator(FrameAllocator&&) = delete;                  ///< @brief Não-movível (posse fixa por thread).
    FrameAllocator& operator=(FrameAllocator&&) = delete;

    /**
     * @brief Aloca por bump; encadeia um chunk (com `VLOG_WARN`) se não couber.
     * @param Size Bytes (> 0; 0 → span vazio). @param Alignment Potência de 2. @param Tag Categoria.
     * @return Span de `Size` bytes válido até o próximo `BeginFrame()`; vazio só se um chunk novo
     *         do tamanho necessário não puder ser adquirido. `noexcept`, nunca aborta.
     */
    Vspan<Vbyte> Allocate(Vuint64 Size, Vuint64 Alignment, MemoryTag Tag) noexcept override;
    void Free(Vspan<Vbyte> Block) noexcept override;            ///< @brief No-op (ver BeginFrame()).

    /// @brief Rebobina o cursor de todos os chunks ao início; reuso no próximo frame. NÃO desaloca.
    void BeginFrame() noexcept;

    /// @brief Bytes em uso no frame atual (soma dos cursores). @return bytes.
    Vuint64 UsedBytes() const noexcept;
    /// @brief Capacidade total somando todos os chunks. @return bytes (>= ChunkBytes).
    Vuint64 CapacityBytes() const noexcept;
    /// @brief Número de chunks possuídos (1 antes de qualquer grow). @return contagem.
    Vuint32 ChunkCount() const noexcept;

private:
    // Lista de chunks possuída por RAII vive no .cpp (PIMPL/membros Private) — sem vazar <vector> no Public.
};
}
```

### Engine/Source/Runtime/Memory/Public/Memory/TrackingAllocator.h
```cpp
#include <Core/Types.h>        // Vuint64, Vbyte, Vspan
#include <Memory/IAllocator.h>
#include <Memory/MemoryTag.h>

namespace VibeEngine
{
/**
 * @brief Wrapper de `IAllocator` que rastreia alocações vivas para diagnóstico de leaks (ADR 0003).
 * @details DELEGA a um `IAllocator&` interno (não-dono — §12). Com `VIBE_TRACKING_ALLOC==1` (Debug e
 *          Development; asan-debug também), registra `{tag, size, thread_id}` por bloco num mapa
 *          protegido por mutex; `Free` remove. Com `VIBE_TRACKING_ALLOC==0` (Shipping) vira pass-through
 *          de custo zero (classe sempre presente; getters retornam 0 / `ReportLeaks()==true`). Bloco
 *          não removido ao shutdown = leak, reportado agrupado por `MemoryTag`. (callsite adiado — ADR 0003 adendo.)
 */
class TrackingAllocator final : public IAllocator
{
public:
    /// @brief Envolve um allocator concreto. @param Inner allocator delegado (não-dono; deve sobreviver a este).
    explicit TrackingAllocator(IAllocator& Inner) noexcept;
    ~TrackingAllocator() override;   ///< @brief Em build com tracking, alerta se houver blocos vivos.

    TrackingAllocator(const TrackingAllocator&) = delete;       ///< @brief Não-copiável.
    TrackingAllocator& operator=(const TrackingAllocator&) = delete;
    TrackingAllocator(TrackingAllocator&&) = delete;            ///< @brief Não-movível.
    TrackingAllocator& operator=(TrackingAllocator&&) = delete;

    Vspan<Vbyte> Allocate(Vuint64 Size, Vuint64 Alignment, MemoryTag Tag) noexcept override; ///< @brief Delega + registra.
    void Free(Vspan<Vbyte> Block) noexcept override;            ///< @brief Delega + desregistra.

    /// @brief Bytes vivos atribuídos a `Tag`. @param Tag categoria @return bytes; 0 se VIBE_TRACKING_ALLOC==0.
    Vuint64 LiveBytes(MemoryTag Tag) const noexcept;
    /// @brief Número de alocações vivas (todas as tags). @return contagem; 0 se VIBE_TRACKING_ALLOC==0.
    Vuint64 LiveAllocationCount() const noexcept;
    /// @brief Emite o relatório de leaks por tag via `VLOG_WARN`. @return true se zero leaks; true se VIBE_TRACKING_ALLOC==0.
    bool ReportLeaks() const noexcept;

private:
    // Mapa {ponteiro → {tag,size,thread_id}} + mutex vivem no .cpp (sob #if VIBE_TRACKING_ALLOC) — sem vazar no Public.
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **Grow do FrameAllocator = lista de chunks encadeados, NUNCA realloc** — realloc invalidaria spans já
  servidos no frame; chunk-list preserva (ADR 0003 "grow"). Pedido > `ChunkBytes` → chunk sob medida
  `max(ChunkBytes, AlignUp(Size,Alignment))`. `VLOG_WARN` UMA vez por grow (não por allocate — evita spam §13).
- **`BeginFrame()`** zera os cursores de todos os chunks; MANTÉM os chunks (sem encolher/realocar — pico residente é o comportamento de arena de frame). `Free` no-op.
- **`new`/`malloc` cru SOMENTE em `FrameAllocator.cpp`** (chunks), confinado a `Memory/` (§12), liberado no destrutor. Sem C-cast (use `static_cast`/`reinterpret_cast`). Alinhamento como na T05; `VASSERT(Alignment potência de 2)`.
- **`TrackingAllocator` NÃO aloca backing** — delega 100% ao `Inner`. Toda a lógica de tracking sob `#if VIBE_TRACKING_ALLOC`; classe sempre presente (EngineCore/smoke compilam igual em todo preset). Mapa = `std::unordered_map` + `std::mutex` no `.cpp` (código frio, permitido §12); `thread_id` via `std::this_thread::get_id()`. `Free(span vazio)`/span não rastreado = no-op seguro.
- **Record sem callsite**: `{tag, size, thread_id}` (decisão da criação — ADR 0003 adendo).
- **VLOG alcançável**: `Vibe::Memory` já linka `Vibe::Core` PUBLIC (T05) → `<Core/Logging.h>` (T03) disponível nos `.cpp` Private.
- **Hook de captura de log (teste #3)**: assinatura exata da T03 (guardada por `#if VIBE_TESTING`, em `<Core/Logging.h>`):
  `void VibeEngine::SetLogCaptureForTesting(void (*Capture)(LogLevel Level, const char* Message));` — o teste instala um callback que conta/guarda as linhas `Warn` e o remove no teardown (`nullptr`).
- **CMake**: `Memory/CMakeLists.txt` (files_modify) ganha `Private/FrameAllocator.cpp` e `Private/TrackingAllocator.cpp` na lista de fontes de `VibeMemory`. NADA mais (subdir e link de teste já feitos na T05).
- **`VIBE_TRACKING_ALLOC==1` em debug/development/asan-debug** (ADR 0008: asan = debug exceto TRACY) → os testes de tracking rodam o caminho completo em todos os presets do gate.

## Plano de testes (lista RED — ordem de execução do `tdd`)

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Memory_Smoke_Task06` | `[memory][smoke]` | Memory_Smoke_Task06.cpp | `LinearAllocator` backing → `TrackingAllocator(inner)`; dirige um `FrameAllocator` por 2 frames (`BeginFrame` rebobina); ciclo alloc/free balanceado via `TrackingAllocator` (visto como `IAllocator&`); `ReportLeaks()==true` e `LiveAllocationCount()==0`; sem leak (asan); < 1 s |
| 2 | `Memory_Frame_AllocAlignReset` | `[memory][unit]` | Memory_Frame.cpp | Allocate align 1/8/16/64 → `data()` alinhado; blocos não se sobrepõem; `UsedBytes()` cresce; `Free` é no-op; `BeginFrame()` reusa o endereço base e zera `UsedBytes()` |
| 3 | `Memory_Frame_GrowChainsWithWarn` | `[memory][unit]` | Memory_Frame.cpp | `FrameAllocator(256)`; allocs além de 256 → `ChunkCount()` e `CapacityBytes()` crescem; ponteiros servidos ANTES do grow continuam válidos (escreve/lê sentinela pós-grow); 1 `VLOG_WARN` capturado via `SetLogCaptureForTesting` |
| 4 | `Memory_Frame_ResetAfterGrowReclaims` | `[memory][unit]` | Memory_Frame.cpp | após grow (`ChunkCount()==2`), `BeginFrame()` mantém `ChunkCount()==2` e `UsedBytes()==0`; nova alloc cabe sem novo grow |
| 5 | `Memory_Frame_PerInstanceIndependence` | `[memory][unit]` | Memory_Frame.cpp | duas instâncias independentes (simula 2 threads): allocs/`BeginFrame` numa não afetam `UsedBytes`/`ChunkCount` da outra (posse per-thread, ADR 0003) |
| 6 | `Memory_Frame_PolymorphicViaIAllocator` | `[memory][unit]` | Memory_Frame.cpp | `FrameAllocator` como `IAllocator&` serve `Allocate` alinhado (span não-vazio) via vtable; `Free` no-op |
| 7 | `Memory_Tracking_RecordsAndReleases` | `[memory][unit]` | Memory_Tracking.cpp | wrap de `LinearAllocator`; `Allocate(100,_,Core)` → `LiveBytes(Core)==100`, `LiveAllocationCount()==1`, `{thread_id}==this_thread`; após `Free` → 0; `ReportLeaks()==true` |
| 8 | `Memory_Tracking_AggregateByTag` | `[memory][unit]` | Memory_Tracking.cpp | allocs Core(2 blocos) + Job(1) → `LiveBytes(Core)`==soma Core, `LiveBytes(Job)`==Job, tag intocada == 0 |
| 9 | `Memory_Tracking_DetectsLeak` | `[memory][unit]` | Memory_Tracking.cpp | alloc sem free → `LiveAllocationCount()==1`, `ReportLeaks()==false`; libera → `ReportLeaks()==true` (define "leak = mapa não-vazio") |

**Smoke**: teste #1, < 30 s. **Determinismo** (HARDENING §7): sem RNG/wall-clock/sleep; padrão de bytes
literal `0xCD`; nenhuma thread spawnada — `thread_id` testado na thread corrente; per-thread provado por
independência de instância (#5). **Cobertura**: cada símbolo público dos 2 contratos aparece em ≥ 1 linha.
**Captura de `VLOG_WARN`** (#3): hook `SetLogCaptureForTesting` da T03 (`#if VIBE_TESTING`) — não criar API nova.
**Tracking on em todos os presets do gate** (debug/dev/asan: `VIBE_TRACKING_ALLOC==1`) → asserções valem.
**Gate de memória**: preset `asan-debug` (`risco_memoria: true`) roda o caminho completo de tracking sob ASan.

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos + gate asan)

```powershell
cmake --preset debug
cmake --preset development
cmake --build --preset debug
cmake --build --preset development
ctest --preset debug --output-on-failure -R "Memory_"
Build\debug\bin\VibeTests.exe "[memory][smoke]" --durations yes
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure

# Gate de memória (risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure -R "Memory_"
```

## Critério de aceitação
- [ ] 9 TEST_CASE do plano verdes — `ctest --preset debug -R "Memory_"` (inclui os 6 da T05; total Memory ≥ 15)
- [ ] Smoke `Memory_Smoke_Task06` < 1 s — saída de `--durations yes`; `ReportLeaks()==true`
- [ ] Suite completa verde em debug e development (não regrediu)
- [ ] **asan-debug verde** — `ctest --preset asan-debug -R "Memory_"` (gate; tracking on sob ASan)
- [ ] `new`/`malloc` cru SOMENTE em `FrameAllocator.cpp`; chunks liberados no destrutor (RAII) — inspeção
- [ ] Headers Public sem include de Private/, `<windows.h>`, spdlog, `<vector>`/`<unordered_map>`/`<mutex>` — inspeção
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T05/T06). **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `FrameAllocator` (ctor, `Allocate`/`Free`, `BeginFrame`, `UsedBytes`/`CapacityBytes`/`ChunkCount`) | T09, T13 |
| `TrackingAllocator` (ctor `IAllocator&`, `LiveBytes`/`LiveAllocationCount`/`ReportLeaks`) | T13, T14 (gate zero-leak do smoke da fase) |

## Hardening aplicável (referência + concretização)
- §3 → aqui: `FrameAllocator`/`TrackingAllocator` PascalCase; membros `m_*`.
- §4 → aqui: headers em `Public/Memory/`; estado pesado (chunks/map/mutex) só no `.cpp`; sem Win32/spdlog.
- §5 → aqui: Doxygen em toda declaração pública.
- §7 → aqui: a tabela do plano É a aplicação; captura de log via hook existente; sem threads.
- §12 → aqui: `new` cru só em `FrameAllocator.cpp` (Memory/); RAII; `Vspan`; sem C-cast; **gate asan-debug**.
- §13 → aqui: `VLOG_WARN` uma vez por grow (não per-frame); instrumentação Tracy do caminho de frame fica na T09.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Dependências (T03 e T05 = Implementado) → 4. Pre-flight
(`IAllocator`/`MemoryTag`/`LinearAllocator` reais vs Tasks/05; `VLOG_*`/`SetLogCaptureForTesting` vs Tasks/03) →
5. (git já bootstrapped) → 6. Status Em-execução → 7. Baseline (suite Memory da T05 verde) → 8. Branch
`task/06-memory-frame-tracking` → 9. `tdd` sobre a lista RED → 10. Gate completo + asan-debug →
11. Status Implementado → 12. Commit `[task 06] memory: frame allocator + tracking`, push, PR.

## Desvios aprovados
(vazio)

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §8.4 · Phases/Fase-01-foundation.md · Decisions/0001, 0003 (+adendo), 0008, 0014
