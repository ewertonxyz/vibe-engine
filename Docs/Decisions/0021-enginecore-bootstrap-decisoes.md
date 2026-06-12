---
id: 0021
titulo: EngineCore bootstrap — ciclo de vida, detecção de HW e topologia de allocators
data: 2026-06-11
status: Aceita
relacionada: [Phases/Fase-01-foundation, Decisions/0003, Decisions/0006, Decisions/0007, Decisions/0012, Tasks/07, Tasks/13, Tasks/14]
---

## Contexto

A T13 (engine-core-bootstrap) materializa `EngineCore::Initialize/Shutdown` (ADR 0006, ADR 0012) e a T14
(vibetests-exe-smoke-fase1) materializa o smoke `[smoke][fase1]`. Ao assar as duas, decisões não fixadas
surgiram (consultados na criação: `vx-spec-architecture`, `vx-spec-memory-perf`, `vx-spec-testing`):

1. ADR 0007 manda `EngineCore::Initialize` checar 8 núcleos físicos via `PlatformThread::GetPhysicalCoreCount()`,
   **mas o contrato da T07 não expôs essa função** (lacuna de baking).
2. Forma da API: o registro da fase mostra funções livres `VibeEngine::Initialize/Shutdown`, mas o critério de
   aceitação da fase diz "EngineCore::Initialize/Shutdown".
3. Topologia de allocators que `EngineCore` possui e o que o gate de zero-leak verifica.
4. Como o smoke observa "TrackingAllocator: 0 leaks" sem expor accessor.
5. Como observar a ordem fixa de boot/teardown (ADR 0006) deterministicamente.

## Decisão

A decisão 1 foi escolhida pelo usuário via `AskUserQuestion`; as demais por fonte citável + recomendação dos especialistas.

- **(1) `GetPhysicalCoreCount` mora na Platform (T07).** Adiciona-se `static Vuint32 PlatformThread::GetPhysicalCoreCount()
  noexcept;` (via `GetLogicalProcessorInformationEx(RelationProcessorCore)`) — detecção de HW é capability de
  plataforma (HARDENING §4: o tipo vive no menor módulo que o possui), mantém `EngineCore` livre de Win32. T07
  (Planejado) é emendado no contrato da fase e no doc da task (adição cirúrgica: 1 função + 1 teste).
- **(2) `class EngineCore` com `static Initialize/Shutdown`.** Resolve a contradição registro (funções livres) vs
  aceitação ("EngineCore::"). Evita o nome genérico colidível `VibeEngine::Initialize`. `EngineCore()=delete`
  (não-instanciável; facade estático, como `JobSystem`). O registro da fase é emendado.
- **(3) Topologia de allocators.** `EngineCore` possui (estado privado num `struct` TU-local em `EngineCore.cpp`,
  RAII): (a) um `LinearAllocator` "system" (1 MiB default, `MemoryTag::Core`); (b) um `TrackingAllocator` que
  envolve o system allocator, ativo só com `m_EnableTracking` e `VIBE_TRACKING_ALLOC` (debug/development;
  pass-through em Shipping); (c) um `FrameAllocator` **por worker** (`WorkerCount()` deles), cada um de
  `m_FrameAllocatorSize` bytes (0 → 1 MiB — ADR 0003/0006). **`EngineCore` linka `Vibe::Core`+`Vibe::Memory`+
  `Vibe::Platform`+`Vibe::JobSystem`, NÃO `Vibe::FileSystem`**: o `FileSystem` é stateless na Fase 1 (sem init/
  shutdown a orquestrar — T11/T12), então seu "passo" no boot é apenas um marcador de log (vira código quando o
  FileSystem ganhar estado global, pós-MVP).
- **(4) Gate de zero-leak via log.** O passo Memory do `Shutdown` chama `TrackingAllocator::ReportLeaks()` e emite
  `VLOG_INFO` com a string canônica **`"TrackingAllocator: <N> leaks"`** (com `N==0` no caminho limpo). O smoke
  arma `SetLogCaptureForTesting` (T03, `#if VIBE_TESTING`) e afirma a captura de `"TrackingAllocator: 0 leaks"`.
  Nenhum accessor público é adicionado (registro mínimo). A string exata é contrato T13↔T14 (mudá-la = rebake).
- **(5) Ordem observável por log.** `Initialize` emite um `VLOG_INFO` marcador por passo na ordem ADR 0006
  (Logging→Memory→Platform→JobSystem→FileSystem); `Shutdown` na ordem inversa. Os testes de ordem capturam a
  sequência via o hook do T03 e a afirmam — seam de logging (produção-realista), não API de teste.
- **(6) HW e erros.** `m_WorkerCount==0` → auto `max(8, GetPhysicalCoreCount()-1)`; < 8 físicos em modo auto →
  `EngineError::InsufficientHardware`. Override manual (`m_WorkerCount≥1`) ignora o gate de HW (ADR 0007). O smoke
  passa `m_WorkerCount=8` explícito (portável em qualquer máquina). O ramo auto-<8 não é testado unitariamente
  (não se força < 8 núcleos em runtime); testa-se a existência do enum + o caminho de override.
  `enum class EngineError : Vuint32 { Unknown, AlreadyInitialized, InsufficientHardware, SubsystemInitFailed }`.

## Consequências

- O registro `## Contratos entre tasks` da fase é emendado em **T07** (adiciona `GetPhysicalCoreCount`) e **T13**
  (forma `class EngineCore`). O doc da T07 (Planejado) recebe a adição cirúrgica; sua execução já a inclui.
- `Initialize` faz **rollback reverso** dos passos concluídos se um passo falhar — nunca deixa estado meio-inicializado.
- Idempotência: `Initialize` com runtime vivo → `AlreadyInitialized`; `Shutdown` sem `Initialize` ou repetido → no-op seguro.
- Nenhum `VPROFILE_ZONE` em `Initialize/Shutdown` (boot não é per-frame, §13); as 4 zonas mandadas vivem em
  JobSystem/FileSystem/WorkerThread (já entregues).
- O gate de zero-leak da Fase 1 (smoke) é, na prática, leve (quase nada aloca pelo `TrackingAllocator` em Fase 1 —
  JobSystem usa heap frio, ADR 0018; FileSystem usa `std::vector`), mas prova que `EngineCore` destrói seus próprios
  allocators sem vazar e fixa o ponto de observação para as fases futuras.
