---
id: 0018
titulo: JobSystem — API pública, ciclo de vida e fronteira T09/T10
data: 2026-06-11
status: Aceita
relacionada: [Phases/Fase-01-foundation, Decisions/0002, Decisions/0006, Decisions/0007, Tasks/09, Tasks/10]
---

## Contexto

O registro `## Contratos entre tasks` da Fase 01 esboça o JobSystem em poucos símbolos
(`struct JobHandle; class JobSystem; // static Schedule(Job)->JobHandle; class JobFence; // void Wait();
void ParallelFor(...); class TaskGraph;`) e o esboço "Tasks previstas" divide o subsistema em
T09 (worker-queue) e T10 (graph-parallelfor). Ao assar as duas tasks, várias decisões de API não
deriváveis diretamente do design surgiram (consultados na criação: `vx-spec-memory-perf`,
`vx-spec-architecture`, `vx-spec-testing`):

1. `JobSystem` é facade **estático** (pool global único) ou instância dona do EngineCore?
2. Onde fica a fronteira **T09 | T10** — em particular, `JobFence` pertence a qual task?
3. Quem carrega o **wait** de um job único: `JobHandle` ou o facade?
4. Como `Schedule` apaga o tipo do callable para o payload inline de 48 B (ADR 0002) mantendo `Job`
   em `Private/`?
5. Assinatura de `ParallelFor`: por índice (`void(Index)`) ou por intervalo (`void(Begin, End)`)?
6. Armazenamento do `TaskGraph` — allocator injetado ou capacidade fixa?
7. `JobSystem::Initialize` recebe um `IAllocator&` ou usa heap frio (`unique_ptr`)?

As decisões 2, 3, 5 puderam ser resolvidas por fonte citável (ordem de build da fase, registro de
contratos, HARDENING §13); a 6 foi decidida pelo usuário via `AskUserQuestion`.

## Opções consideradas

- **(1) Facade**: (A) estático global; (B) instância dona do EngineCore.
- **(2) Fronteira**: (A) `JobFence` em T09 com o facade; (B) `JobFence` em T10 (como no esboço).
- **(3) Wait**: (A) `JobSystem::Wait(JobHandle)`; (B) `JobHandle::Wait()`.
- **(5) ParallelFor**: (A) por intervalo `void(Begin,End)`; (B) por índice `void(Index)`.
- **(6) TaskGraph storage**: (A) `IAllocator&` no ctor; (B) `TaskGraph(Vuint32 MaxNodes)` fixo.
- **(7) Initialize**: (A) `IAllocator&` injetado; (B) heap frio `unique_ptr`.

## Decisão

- **(1) = A, facade estático.** Bate com o registro da fase (`static Schedule`) e com §5.4. `EngineCore`
  (T13, ADR 0006) dirige o lifetime chamando `JobSystem::Initialize(WorkerCount)` / `JobSystem::Shutdown()`
  em ordem fixa/reversa, sem possuir o objeto. `IsInitialized()` permite isolar fixtures de teste.
  Tipos no namespace raiz `VibeEngine` (consistente com Core/Memory/Platform da Fase 1).
- **(2) = A, `JobFence` em T09.** A ordem de build da fase (§Ordem interna passo 6:
  WorkerThread → Job → JobQueue → JobHandle → JobFence → ParallelFor → TaskGraph) coloca `JobFence`
  antes de `ParallelFor`/`TaskGraph`. `Schedule` precisa de um resultado aguardável para ser testável em
  T09. **Emenda o esboço não-vinculante "Tasks previstas"** (que listava `JobFence` em T10).
  Cut final: **T09** = `JobHandle`, `JobFence`, facade `JobSystem`; **T10** = `ParallelFor`, `TaskGraph`.
- **(3) = A, `JobSystem::Wait(JobHandle)`.** `JobHandle` permanece POD opaco (índice + geração, §12),
  sem dependência de pool/threading no header — incluível em qualquer lugar.
- **(4)** `Schedule<Fn>` é um template **Public** inline que faz
  `static_assert(sizeof(decay_t<Fn>) <= 48)` + `is_trivially_copyable` + `is_trivially_destructible`
  (ADR 0002, falha de compilação em captura > 48 B) e delega a um `ScheduleImpl` **Private**
  recebendo `(void(*)(void*) Entry, Vspan<const Vbyte> Payload)`. O thunk `Detail::JobThunk<Fn>`
  reinterpreta o payload e invoca. Nenhum símbolo de `Job`/`JobQueue`/`WorkerThread`/`PlatformThread`/
  `<thread>` aparece em header Public (§4). Interface interna usa `Vspan`, nunca ptr+size (§12).
- **(5) = A, `void(Vuint32 Begin, Vuint32 End)` por intervalo.** Amortiza o custo de chamada por
  iteração e permite vetorização do laço interno (HARDENING §13). Assinatura:
  `ParallelFor(Vuint32 Count, Vuint32 ChunkSize, Fn&& Body)`; `ChunkSize == 0` ⇒ divisão estática
  uniforme `ceil(Count / WorkerCount())`.
- **(6) = A, `TaskGraph(IAllocator& Allocator)`** (decisão do usuário). Nós/arestas via allocator da
  engine taggeados `MemoryTag::Job` (que existe exatamente para isto, T05); alinhado ao pilar de
  memória (§12). Grafo **single-shot**: `Submit()` consome o grafo e retorna `JobFence`; não-reutilizável.
- **(7) = B, heap frio `unique_ptr` em `Initialize`** (recomendação `vx-spec-memory-perf`). `Initialize`
  é código frio de boot único; §12 permite `unique_ptr`/containers padrão em código frio. A única
  alocação quente — o array de `Job` por worker — é o ring de capacidade fixa do `JobQueue`, irrelevante
  para allocator. Injetar um allocator só acoplaria a ordem de init de Memory (ADR 0006) sem ganho
  mensurável. Consequência: **T09 não linka `Vibe::Memory`** (só `Vibe::Core` + `Vibe::Platform`); o
  link de `Vibe::Memory` ao alvo `VibeJobSystem` entra com a **T10** (que usa `IAllocator` no TaskGraph).

## Consequências

- O registro `## Contratos entre tasks` da Fase 01 é **emendado** (HARDENING §14) para refletir a
  superfície elaborada: `JobSystem::Initialize/Shutdown/IsInitialized/WorkerCount`,
  `Schedule(Fn)->JobHandle`, `Schedule(Fn, JobFence&)`, `Wait(JobHandle)`/`IsComplete(JobHandle)`,
  `JobFence::Wait/IsComplete`, `ParallelFor(Count, ChunkSize, Body)`,
  `TaskGraph(IAllocator&)/AddNode/AddDependency/Submit`.
- `JobFence` ganha caminho de construção em T09 via **associação**: `JobSystem::Schedule(fn, JobFence&)`
  incrementa o contador da fence; o worker decrementa ao completar; `Wait()` é cooperativo (ajuda o pool,
  não fica em spin ocioso). `ParallelFor` e `TaskGraph::Submit` reusam o mesmo mecanismo.
- O alvo do módulo é `VibeJobSystem` / alias `Vibe::JobSystem` (ADR 0014); includes `<JobSystem/Foo.h>`.
- Pós-MVP: trocar o backend de worker por fibers (§5.4) muda apenas `WorkerThread`/scheduling Private;
  a API pública aqui fixada permanece.
