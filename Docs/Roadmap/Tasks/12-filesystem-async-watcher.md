---
id: 12
nome: filesystem-async-watcher
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [09, 11]
decisions: [0002, 0005, 0014, 0020]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: true
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0002-jobsystem-worker-pool.md
  - Docs/Decisions/0005-infra-de-testes.md
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Decisions/0020-filesystem-leitura-e-watcher.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md          # §Contratos entre tasks (T11/T12), §Comandos canônicos
  - Docs/Roadmap/Tasks/09-jobsystem-worker-queue.md    # apenas §Contratos (JobSystem::Schedule, JobFence)
  - Docs/Roadmap/Tasks/11-filesystem-sync-path.md      # apenas §Contratos (FileSystem::ReadSync, Path, FileError)
files_create:
  - Engine/Source/Runtime/FileSystem/Public/FileSystem/AsyncReadRequest.h
  - Engine/Source/Runtime/FileSystem/Public/FileSystem/FileWatcher.h
  - Engine/Source/Runtime/FileSystem/Private/ReadState.h
  - Engine/Source/Runtime/FileSystem/Private/AsyncReadRequest.cpp
  - Engine/Source/Runtime/FileSystem/Private/Windows/FileWatcherWindows.cpp
  - Engine/Source/Runtime/FileSystem/Tests/FileSystem_ReadAsync.cpp
  - Engine/Source/Runtime/FileSystem/Tests/FileSystem_Watcher.cpp
files_modify:
  - Engine/Source/Runtime/FileSystem/Public/FileSystem/FileSystem.h
  - Engine/Source/Runtime/FileSystem/CMakeLists.txt
---

# 12 — FileSystem: ReadAsync + FileWatcher

## Objetivo
Segunda fatia do `FileSystem` (design-mvp.md §9 "read async, file watcher"): `AsyncReadRequest` (leitura
assíncrona via `JobSystem`, move-only, sem use-after-free), `FileSystem::ReadAsync`, e `FileWatcher`
(`ReadDirectoryChangesW` drenado por `Poll()` na thread do chamador — modelo Poll-unificado, ADR 0020).
Reusa `FileSystem::ReadSync` (T11) no job e `JobSystem`/`JobFence` (T09). O executor NÃO precisa ler o design.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso**
(HARDENING §14). T09 e T11 ainda não estão implementadas na criação → o pre-flight compara os contratos
contra `Tasks/09 §Contratos` e `Tasks/11 §Contratos`; ao executar, o gate de dependências garante os headers reais.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4, §5, §7, §12, §14 |
| Decisions/0002 | Schedule: captura ≤ 48 B trivialmente copiável → job captura `ReadState*` (submitter aloca) | inteiro |
| Decisions/0005 | `[filesystem]`; `Poll()` em teste (refinado por 0020 para incondicional) | inteiro |
| Decisions/0014 | include `<FileSystem/Foo.h>` / `<JobSystem/Foo.h>` / `<Core/Foo.h>` | inteiro |
| Decisions/0020 | AsyncReadRequest lifetime; Cancel CAS; FileWatcher Poll-unificado (ReadDirectoryChangesW+OVERLAPPED, sem thread) | inteiro |
| Phases/Fase-01 | contrato T11/T12 + comandos (asan) | §Contratos entre tasks, §Comandos canônicos |
| Tasks/09 | `JobSystem::Schedule(Fn&&, JobFence&)`; `JobFence` (Wait/IsComplete); `WorkerCount`; `Initialize/Shutdown` | §Contratos |
| Tasks/11 | `FileSystem::ReadSync`, `Path`, `FileError` (incl. `Cancelled`) | §Contratos |

## Escopo

### Dentro
- 2 headers Public em `Public/FileSystem/`, 1 header + 1 TU Private, 1 TU Windows, 2 arquivos de teste.
- `AsyncReadRequest` move-only: `Wait`/`Cancel`/`Result`; dono de `unique_ptr<ReadState>`; dtor faz `Wait()`.
- `FileSystem::ReadAsync` (adicionado ao header da T11): agenda um job que chama `ReadSync` para o `ReadState`.
- `FileWatcher`: `Watch(Path, Callback, UserData)` + `Poll()` (incondicional); backend Win32 em Private.
- Adicionar `Vibe::JobSystem` ao link de `VibeFileSystem`; `VPROFILE_ZONE("FileSystem.ReadAsync")`.

### Fora (NÃO fazer, mesmo que pareça óbvio)
- Thread dedicada de watcher (modelo Poll-unificado dispensa — ADR 0020). `Poll()` sob `#if VIBE_TESTING` (agora incondicional — ADR 0020 refina ADR 0005).
- `std::function` no callback (usar `void(*)(const Path&, void*)` — §13). Captura do buffer/vector inline no job (capturar só `ReadState*` — ADR 0002).
- Alterar a lógica de `ReadSync`/`Path`/`FileError` (T11). Link de `Vibe::Memory`. Escrita de arquivo.

### Semântica vinculante dos arquivos (HARDENING §14)
`files_modify`: `FileSystem.h` (adicionar `ReadAsync`) + `FileSystem/CMakeLists.txt` (link `Vibe::JobSystem` +
fontes async/watcher + define `VIBE_TESTING` só no alvo de teste). Exceções permanentes: este doc, README,
Backlog. `Engine/Source/Tests/CMakeLists.txt` NÃO entra (T11 já linkou `Vibe::FileSystem`; fontes auto-globadas
— ADR 0016). Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE quando: (a) contrato não compila vs `<JobSystem/...>`/`<FileSystem/...>` reais (drift T09/T11); (b)
precisar de arquivo fora das listas; (c) seções se contradizem; (d) teste impossível; (e) gate (incl. asan)
falha após 2 ciclos de `diagnose`. Relate: **passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <FileSystem/Foo.h>`. Doxygen §5 completo.)

### Public/FileSystem/AsyncReadRequest.h
```cpp
#include <Core/Result.h>   // VResult
#include <Core/Types.h>    // Vbyte
#include <FileSystem/FileError.h>

#include <memory>          // std::unique_ptr (PImpl do ReadState)
#include <vector>

namespace VibeEngine
{
struct ReadState;   ///< @brief Bloco de controle async (Private/ReadState.h); tamanho opaco aqui.

/**
 * @brief Handle move-only de uma leitura assíncrona; dono exclusivo do bloco de controle (ADR 0020).
 * @details O job de leitura captura apenas um `ReadState*` de 8 B (ADR 0002); o bloco contém o buffer,
 *          a `JobFence` e o status atômico. O dtor faz `Wait()` antes de liberar — sem use-after-free se
 *          o request for destruído/movido antes do job rodar.
 */
class AsyncReadRequest
{
public:
    AsyncReadRequest() noexcept;                                  ///< @brief Request vazio (sem bloco).
    ~AsyncReadRequest();                                          ///< @brief `Wait()` e libera o bloco (definido no .cpp — ReadState incompleto).
    AsyncReadRequest(AsyncReadRequest&&) noexcept;                ///< @brief Move: transfere o bloco; origem fica vazia.
    AsyncReadRequest& operator=(AsyncReadRequest&&) noexcept;     ///< @brief Move-assign: `Wait()`+libera o atual antes de absorver.
    AsyncReadRequest(const AsyncReadRequest&) = delete;
    AsyncReadRequest& operator=(const AsyncReadRequest&) = delete;

    /// @brief Bloqueia até a leitura concluir/cancelar/falhar. Idempotente. No-op se vazio.
    void Wait() noexcept;
    /// @brief Solicita cancelamento (CAS atômico Pending→Cancelled). @return true se cancelou antes de iniciar; false se já rodando/concluído.
    bool Cancel() noexcept;
    /// @brief Resultado da leitura; chama `Wait()` se ainda pendente. @return bytes, ou `FileError` (`Cancelled` se cancelado).
    VResult<std::vector<Vbyte>, FileError> Result();

private:
    friend class FileSystem;
    explicit AsyncReadRequest(std::unique_ptr<ReadState> State) noexcept;   ///< @brief Construído por `FileSystem::ReadAsync`.
    std::unique_ptr<ReadState> m_State;   ///< @brief Bloco de controle (buffer+fence+status+path+erro).
};
}
```

### Public/FileSystem/FileWatcher.h
```cpp
#include <Core/Result.h>   // VResult
#include <FileSystem/FileError.h>
#include <FileSystem/Path.h>

#include <memory>          // std::unique_ptr (estado por-watch)
#include <vector>

namespace VibeEngine
{
/// @brief Callback de evento do watcher: ponteiro de função + user-data (sem `std::function` — §13).
using FileWatchCallback = void (*)(const Path& ChangedPath, void* UserData);

/**
 * @brief Observa diretórios por mudanças via `ReadDirectoryChangesW`+`OVERLAPPED` (Private), drenado por `Poll()`.
 * @details Modelo Poll-unificado (ADR 0020, refina ADR 0005): a I/O assíncrona ENFILEIRA; `Poll()` dispara os
 *          callbacks na thread do chamador (frame loop em produção; teste nos testes). Sem thread dedicada.
 */
class FileWatcher
{
public:
    FileWatcher();
    ~FileWatcher();                                  ///< @brief Cancela I/O pendente e libera buffers/OVERLAPPED (RAII; definido no .cpp).
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    /// @brief Registra observação de um diretório. @param DirPath diretório @param Callback função @param UserData ponteiro opaco (não-dono). @return ok ou FileError.
    VResult<void, FileError> Watch(const Path& DirPath, FileWatchCallback Callback, void* UserData);
    /// @brief Drena eventos pendentes e dispara callbacks na thread do chamador. Não-bloqueante (ADR 0020).
    void Poll();

private:
    struct WatchState;                                            ///< @brief Estado por-diretório (Private): OVERLAPPED + buffer fixo + HANDLE.
    std::vector<std::unique_ptr<WatchState>> m_Watches;          ///< @brief Watches ativos (cada um RAII).
};
}
```

### Modificação em Public/FileSystem/FileSystem.h (files_modify) — adicionar à classe `FileSystem`:
```cpp
#include <FileSystem/AsyncReadRequest.h>   // novo include
// ... dentro de class FileSystem:
    /// @brief Agenda uma leitura assíncrona no JobSystem. @param FilePath arquivo. @return handle move-only (zona Tracy "FileSystem.ReadAsync").
    static AsyncReadRequest ReadAsync(const Path& FilePath);
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **`ReadState` (Private/ReadState.h)** contém: `std::vector<Vbyte> m_Buffer`, `JobFence m_Fence` (T09),
  `std::atomic<Vuint8> m_Status` (Pending=0/Running/Done/Cancelled/Failed), `Path m_Path`, `FileError m_Error`.
- **`ReadAsync`**: `auto State = std::make_unique<ReadState>(...)` (submitter aloca — ADR 0002); captura
  `ReadState* Raw = State.get()` (8 B, trivialmente copiável → passa a guarda de `Schedule` da T09);
  `VPROFILE_ZONE("FileSystem.ReadAsync")`; `JobSystem::Schedule([Raw]() noexcept { ... }, Raw->m_Fence)`;
  retorna `AsyncReadRequest(std::move(State))`. O job: CAS `Pending→Running` (se falhar por `Cancelled`,
  retorna cedo, mas a fence ainda sinaliza); senão chama `FileSystem::ReadSync(Raw->m_Path)` → preenche
  `m_Buffer`/`m_Error`, seta `Done`/`Failed`. **Nunca** captura o vector inline (ADR 0002).
- **Segurança do dtor/move (sem UAF)**: `AsyncReadRequest` é o ÚNICO dono do `unique_ptr`; o job tem só um
  `ReadState*` não-dono. O dtor faz `if (m_State) m_State->m_Fence.Wait()` ANTES do `unique_ptr` liberar —
  o job nunca toca memória já liberada. Move zera `m_State` da origem (dtor no-op). `~AsyncReadRequest` e os
  move-ops são **declarados no header e definidos no `.cpp`** (`ReadState` incompleto no header).
- **`Cancel()` race-safe**: CAS `Pending→Cancelled`. Vence (job não começou) → retorna true; o job, ao
  iniciar, vê `Cancelled` e retorna sem ler, **mas sinaliza a fence** (o `Wait()` do dtor nunca trava).
  CAS falha (já `Running`/`Done`) → retorna false. `Result()` de cancelado → `FileError::Cancelled`.
- **`FileWatcher` (Private/Windows/FileWatcherWindows.cpp)**: `Watch` abre o diretório com `CreateFileW`
  (`FILE_FLAG_OVERLAPPED|FILE_FLAG_BACKUP_SEMANTICS`) e arma `ReadDirectoryChangesW` num buffer fixo de
  **64 KiB** + `OVERLAPPED` por `WatchState` (RAII). `Poll()` chama `GetOverlappedResult(..., FALSE)` (sem
  espera); se completou, parseia `FILE_NOTIFY_INFORMATION`, dispara o callback com o `Path` mudado, e re-arma.
  Dtor de `WatchState`: `CancelIoEx` + drena a OVERLAPPED + `CloseHandle` antes de liberar o buffer (ordem RAII).
  `<windows.h>` (`WIN32_LEAN_AND_MEAN`/`NOMINMAX`) só neste `.cpp`; conversão UTF-8→wide aqui. Sem thread.
- **Sem `new`/`malloc` cru** (FileSystem não é Memory): `std::make_unique`/`std::vector` em código frio (§12). Sem C-cast (`static_cast`/`reinterpret_cast`).
- **CMake** (`FileSystem/CMakeLists.txt`, files_modify): adicionar `Private/AsyncReadRequest.cpp` e
  `Private/Windows/FileWatcherWindows.cpp` às fontes de `VibeFileSystem`; adicionar `Vibe::JobSystem` ao
  `target_link_libraries(... PUBLIC ...)`; `VIBE_TESTING` continua definido só no alvo `VibeTests` (T01/ADR 0005).

## Plano de testes (lista RED — ordem de execução do `tdd`)
Fixture `AsyncFileTestFixture`: temp file via STL + `JobSystem::Initialize(N)` no setup / `Shutdown()` no
teardown. Sincronização só via `AsyncReadRequest::Wait()`/`JobFence`/`FileWatcher::Poll()` — **sem `sleep_for`**.

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `FileSystem_Smoke_Task12` | `[filesystem][smoke]` | FileSystem_ReadAsync.cpp | `Initialize(8)`; `auto r=ReadAsync(temp); r.Wait();` `r.Result()` bate com `ReadSync(temp)`; `FileWatcher.Watch(dir)` + modifica + `Poll()` → callback com Path certo; `Shutdown()`; sem leak (asan); < 1 s |
| 2 | `FileSystem_ReadAsync_Completes` | `[filesystem][integration]` | FileSystem_ReadAsync.cpp | `ReadAsync`→`Wait()`→`Result()` ok e bytes == conteúdo do fixture (sincroniza só por Wait) |
| 3 | `FileSystem_ReadAsync_CancelBeforeStart` | `[filesystem][unit]` | FileSystem_ReadAsync.cpp | `Initialize(1)` + 1 job "bloqueador" ocupa o único worker (spin numa flag atômica); `ReadAsync` fica enfileirado; `Cancel()`==true; libera o bloqueador; `Wait()`; `Result()`→`FileError::Cancelled` (determinístico, sem timing) |
| 4 | `FileSystem_ReadAsync_CancelAfterComplete` | `[filesystem][unit]` | FileSystem_ReadAsync.cpp | `Wait()` até concluir; `Cancel()`==false (tarde demais); `Result()` ok com bytes; sem double-free (asan) |
| 5 | `FileSystem_ReadAsync_DestroyBeforeComplete` | `[filesystem][integration]` | FileSystem_ReadAsync.cpp | `{ auto r=ReadAsync(temp); }` (dtor faz `Wait()`) com JobSystem rodando → asan sem UAF/leak (o job não escreve em memória liberada) |
| 6 | `FileSystem_ReadAsync_MoveTransfersOwnership` | `[filesystem][unit]` | FileSystem_ReadAsync.cpp | move-construct: origem `m_State==nullptr` (Result não-ok/vazio), destino entrega `Result()`; sem double-free (asan) |
| 7 | `FileSystem_Watcher_DetectsModify` | `[filesystem][integration]` | FileSystem_Watcher.cpp | `Watch(dir, cb, &user)`; escreve/modifica arquivo no dir; `Poll()`; callback disparou (≥ 1×) com `Changed`==arquivo e `user` round-trip |
| 8 | `FileSystem_Watcher_NoChangeNoFire` | `[filesystem][unit]` | FileSystem_Watcher.cpp | `Watch(...)` + `Poll()` sem nenhuma mudança → callback NÃO dispara (contador == 0) |

**Smoke**: teste #1, < 30 s. **Determinismo** (HARDENING §7): async sincroniza por `Wait()`/`JobFence`;
watcher por `Poll()` (ADR 0005/0020), sem dormir/timeout; cancel-before-start forçado por job bloqueador +
`Initialize(1)` (sem timing); temp files únicos apagados no teardown. **Gate de memória**: `risco_memoria: true`
→ **asan-debug** é o veredito de UAF/double-free (os testes #3/#5/#6 e o dtor do watcher são os alvos);
TrackingAllocator N/A (FileSystem não linka Memory — precedente T07). **risco_frame false** → sem teste `[perf]`.
**Cobertura**: cada símbolo público (`AsyncReadRequest` Wait/Cancel/Result/move, `ReadAsync`, `FileWatcher`
Watch/Poll, `FileWatchCallback`) aparece em ≥ 1 linha.

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos + gate asan)

```powershell
# Configurar + build (gate: /W4 /WX falham com warning novo)
cmake --preset debug; cmake --preset development
cmake --build --preset debug; cmake --build --preset development
# Testes da task (lista RED)
ctest --preset debug --output-on-failure -R "FileSystem_"
# Smoke da task (com medição de duração)
Build\debug\bin\VibeTests.exe "[filesystem][smoke]" --durations yes
# Suite completa (gate final)
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
# Gate de memória (risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure -R "FileSystem_"
```

## Critério de aceitação
- [ ] 8 TEST_CASE do plano verdes — `ctest --preset debug -R "FileSystem_"` (inclui os 7 da T11; total `[filesystem]` ≥ 15)
- [ ] Smoke `FileSystem_Smoke_Task12` < 30 s (alvo < 1 s) — `--durations yes`
- [ ] Suite completa verde em debug e development (não regrediu; ReadSync da T11 intacto)
- [ ] **asan-debug verde** — `ctest --preset asan-debug -R "FileSystem_"` (UAF/double-free do async + dtor do watcher)
- [ ] Job async captura só `ReadState*` (≤ 48 B, ADR 0002); dtor faz `Wait()` antes de liberar — inspeção
- [ ] `FileWatchCallback` é fn-ptr (sem `std::function`); `<windows.h>` só em `FileWatcherWindows.cpp` — inspeção
- [ ] `VPROFILE_ZONE("FileSystem.ReadAsync")` presente — inspeção (ADR 0004)
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T11/T12), ADR 0020. **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `AsyncReadRequest` (`Wait`/`Cancel`/`Result`), `FileSystem::ReadAsync` | T13, T14 (smoke da fase: read async) |
| `FileWatcher` (`Watch`/`Poll`), `FileWatchCallback` | T13, T14 e Fase 5 (hot reload) |

## Hardening aplicável (referência + concretização)
- §3/§4/§5 → aqui: PascalCase + `m_*`; headers em `Public/FileSystem/`; `ReadState`/`WatchState`/Win32 em Private (sem `<windows.h>` em Public); Doxygen completo (incl. `FileWatchCallback`).
- §7 → aqui: a tabela do plano É a aplicação; barreira = `Wait`/`JobFence`/`Poll`; cancel-before-start forçado sem timing.
- §12 → aqui: `unique_ptr`/`std::vector` em cold path; ownership único (job = ponteiro não-dono); dtor `Wait()` previne UAF; sem C-cast; **gate asan-debug**.
- §13 → aqui: callback = fn-ptr (sem `std::function`); `VPROFILE_ZONE` em ReadAsync; watcher sem thread (Poll-driven).

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
Formato → orçamento → deps (T09/T11 = Implementado) → pre-flight (`JobSystem::Schedule`/`JobFence` vs Tasks/09;
`ReadSync`/`Path`/`FileError` vs Tasks/11) → Em-execução → baseline (suite `[filesystem]` da T11 verde) →
branch `task/12-filesystem-async-watcher` → `tdd` sobre a lista RED → gate completo + asan-debug → Implementado →
commit `[task 12] filesystem: readasync + filewatcher`, push, PR.

## Desvios aprovados
(vazio na criação)

## Bloqueio
(vazio)
## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §9 · Phases/Fase-01-foundation.md · Decisions/0002, 0005, 0014, 0020
