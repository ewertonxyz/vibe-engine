---
id: 0019
titulo: Política de testes de concorrência e [perf] do JobSystem
data: 2026-06-11
status: Aceita
relacionada: [Phases/Fase-01-foundation, Decisions/0004, Decisions/0005, Tasks/09, Tasks/10]
---

## Contexto

T09 e T10 carregam `risco_memoria: true` e `risco_frame: true` (Flags de risco da Fase 01). O §11 do
design exige, para "Job System pronto": "N workers escalonados por núcleo físico", "ParallelFor escala em
throughput" e "TaskGraph com fences resolve dependências". A fase ainda pede "JobSystem sem flakiness em
100 execuções consecutivas". Há três tensões a resolver antes de assar as tasks:

1. HARDENING §13 **exige** um teste `[perf]` com teto de tempo explícito para `risco_frame`, mas também
   proíbe "citar tempo fixo sem captura Tracy/PIX". Como afirmar "escala em throughput" sem flakiness?
2. HARDENING §7 exige determinismo (sem wall-clock no caminho de correção, sem RNG sem seed, sem
   threading sem barreira). Como testar concorrência deterministicamente?
3. "Sem flakiness em 100 execuções" sem CI montada — garantir no repo ou via política?

## Opções consideradas

- **(1) [perf]**: (A) teto absoluto generoso + invariante de correção exato; (B) só correção +
  paralelo ≤ serial (sem teto absoluto); (C) teto absoluto estrito.
- **(3) Anti-flaky**: (A) teste `[job][stress]` no repo repetindo o caminho feliz; (B) política
  `ctest --repeat` / CI registrada em ADR, sem loop no repo.

## Decisão

Decisões 1 e 3 escolhidas pelo usuário via `AskUserQuestion`; a 2 resolvida por fonte citável (§7).

- **(1) = A.** Cada teste `[perf]` afirma um **invariante de correção EXATO** (soma paralela `==` soma
  serial; cada índice escrito exatamente 1×; contador atômico `==` N) **E** um **teto de wall-clock
  absoluto GENEROSO**, tratado como limite superior suave e **provisório**, a ser calibrado por medição
  (Tracy) na execução — ajuste registrado em `## Desvios aprovados` da task. T10 afirma adicionalmente a
  propriedade relativa **paralelo ≤ serial**. Cumpre o "teto explícito" do §13 e é robusto em CI:
  nenhum *ratio* de speedup é exigido (fonte de flakiness). O wall-clock só aparece em testes `[perf]`,
  nunca em asserção de correção `[unit]`/`[smoke]`/`[integration]`.
- **(2)** Concorrência é verificada por **invariantes pós-barreira** sobre `std::atomic`/mapas de escrita
  por índice; a barreira que valida a asserção é o próprio `Wait`/`JobFence::Wait`/`Submit().Wait()` do
  SUT (§7 "threading sem barreira" → o join do SUT É a barreira). Proibido `sleep_for`/`yield`-timing.
  Detecção de data race é delegada ao preset **asan-debug** (`risco_memoria`, §12): o teste `_NoDataRace`
  só fornece carga de escrita disjunta + invariante de correção; o veredito de corrida vem do job asan.
  RNG sempre `std::mt19937` com seed literal fixa (`0xC0FFEEu`); reduções inteiras com `==` exato.
- **(3) = A.** Cada task inclui um teste `[job][stress]` que repete o caminho feliz N× (jobs triviais,
  determinístico, sem timing) dentro de um processo — garantia in-repo imediata, fora do orçamento de
  smoke (< 30 s). Complementado por nota recomendando `ctest --repeat until-pass:100` para cobertura
  por-processo quando houver CI.

## Consequências

- T09 e T10 ganham, cada uma, exatamente um teste `[perf]` (teto generoso + correção exata) e um teste
  `[job][stress]`, além do `[smoke]` e dos `[unit]`/`[integration]`.
- Tetos `[perf]` entram como `constexpr` nomeado generoso no teste, com comentário "provisório —
  calibrar por medição"; nenhum número é citado como fixo definitivo (§13).
- `VPROFILE_ZONE` obrigatório nos caminhos medidos (`JobSystem.Schedule`, `WorkerThread.Run`,
  `JobSystem.ParallelFor`) desde a criação (ADR 0004); um `VPROFILE_PLOT` opcional de overflow de fila
  expõe se a capacidade do ring foi subdimensionada.
- Esta política é o precedente para futuras tasks de sistemas concorrentes/per-frame; estende ADR 0005
  (infra de testes) e ADR 0004 (Tracy).
