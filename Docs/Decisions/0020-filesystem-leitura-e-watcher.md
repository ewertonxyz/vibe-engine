---
id: 0020
titulo: FileSystem — formas de leitura e modelo de entrega do FileWatcher
data: 2026-06-11
status: Aceita
relacionada: [Phases/Fase-01-foundation, Decisions/0002, Decisions/0005, Tasks/11, Tasks/12]
---

## Contexto

As tasks T11 (filesystem-sync-path) e T12 (filesystem-async-watcher) materializam o módulo
`FileSystem` (design §9: "read sync, read async, file watcher"). O registro de contratos da fase fixa
`FileSystem::ReadSync(Path) -> VResult<std::vector<Vbyte>, FileError>`, `AsyncReadRequest` (wrapper sobre
`JobFence`) e `FileWatcher` com `Poll()` apenas em `#if VIBE_TESTING` (ADR 0005). Duas decisões não
estavam fixadas e os especialistas divergiram (consultados na criação: `vx-spec-architecture`,
`vx-spec-memory-perf`, `vx-spec-testing`):

1. **Forma de leitura síncrona**: além da que retorna `std::vector<Vbyte>` (registry), deve existir uma
   sobrecarga zero-alloc que lê para um buffer do chamador (forma §12-preferida)?
2. **Entrega de eventos do `FileWatcher` em produção**: ADR 0005 chama o modelo de "callback assíncrono"
   mas não fixa a thread. Thread dedicada disparando callbacks vs. drain via `Poll()` chamado pelo dono?

## Opções consideradas

- **(1) ReadSync**: (A) só `std::vector<Vbyte>` (registry, YAGNI); (B) adicionar sobrecarga
  `ReadSync(const Path&, Vspan<Vbyte> Dst) -> VResult<Vuint64, FileError>`.
- **(2) FileWatcher**: (A) `Poll()` unificado — thread/IO assíncrono só ENFILEIRA; callbacks disparam no
  drain chamado pelo dono (frame loop em produção, teste nos testes); (B) callback assíncrono — thread do
  watcher invoca o callback diretamente em produção, `Poll()` `#if VIBE_TESTING` só nos testes (ADR 0005 literal).

## Decisão

Ambas escolhidas pelo usuário via `AskUserQuestion`.

- **(1) = B.** `FileSystem` expõe DUAS formas de `ReadSync`:
  - `static VResult<std::vector<Vbyte>, FileError> ReadSync(const Path&)` — buffer próprio (cold path).
  - `static VResult<Vuint64, FileError> ReadSync(const Path&, Vspan<Vbyte> Dst)` — **zero-alloc**, buffer
    do chamador (forma §12-preferida; recebe `Vspan`, não ptr+size). Lê até `Dst.size()` bytes; se o
    arquivo for maior que `Dst.size()`, retorna `FileError::InvalidArgument` (sem truncamento silencioso).
    Detecção de "maior": após preencher `Dst`, uma leitura-sonda de 1 byte; se vier byte, o arquivo excede.
  Dá ao AssetSystem (Fase 5) um caminho sem alocação desde já. Emenda o registro (sobrecarga adicional).
- **(2) = A, `Poll()` unificado.** O backend Win32 usa `ReadDirectoryChangesW` com `OVERLAPPED`
  (`FILE_FLAG_OVERLAPPED`); `Watch()` arma a leitura assíncrona; `Poll()` chama `GetOverlappedResult`
  **sem espera** (`bWait=FALSE`), drena as `FILE_NOTIFY_INFORMATION` completadas, dispara os callbacks na
  **thread do chamador** e re-arma. Em produção o dono chama `Poll()` no frame loop; nos testes o teste
  chama `Poll()` (determinístico, sem dormir — ADR 0005). **Consequência: `Poll()` deixa de ser
  `#if VIBE_TESTING`** e vira API pública incondicional. **Nenhuma thread dedicada de watcher** — o modelo
  Poll-driven elimina o callback cross-thread e o hazard de reentrância em gameplay (§13).
  Callback = ponteiro de função `void(*)(const Path&, void*)` + user-data, nunca `std::function` (§13).

## Consequências

- **Refina o ADR 0005** (que permanece Aceita pelo restante — `VibeTests` único, tag `[filesystem]`): o
  `FileWatcher` em produção NÃO usa callback em thread própria; usa `Poll()` no frame loop. O guard
  `#if VIBE_TESTING` em `Poll()` é removido (método incondicional). O registro de contratos da fase é
  emendado de acordo (HARDENING §14).
- **`AsyncReadRequest` lifetime (T12)**: o job de leitura captura apenas um `ReadState*` de 8 B
  (trivialmente copiável, ≤ 48 B — **ADR 0002**, submitter aloca o bloco); o bloco contém o buffer
  resultado, a `JobFence`, um status atômico, o `Path` e o `FileError`. O dtor de `AsyncReadRequest` faz
  `Wait()` antes de liberar o bloco — sem use-after-free se destruído/movido antes do job rodar. `Cancel()`
  é CAS atômico `Pending→Cancelled`; o job sempre sinaliza a fence (mesmo cancelado) para o `Wait()` do
  dtor nunca travar.
- **`FileError`** ganha o valor `Cancelled` (além de espelhar `PlatformError`), usado por `Result()` de
  uma leitura cancelada antes do início.
- `FileSystem` linka `Vibe::Core`+`Vibe::Platform` (T11) e adiciona `Vibe::JobSystem` (T12); nunca
  `Vibe::Memory` (mermaid da fase). Win32 confinado a `Private/Windows/` (sem `<windows.h>` em Public).
- Post-MVP: a forma `Vspan` é o caminho de leitura sem alocação que o AssetSystem reusa; a entrega
  por-thread do watcher (se algum dia necessária) seria nova ADR — não está planejada.
