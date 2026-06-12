---
id: 14
nome: vibetests-exe-smoke-fase1
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [12, 13]
decisions: [0005, 0021]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: false
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0005-infra-de-testes.md
  - Docs/Decisions/0021-enginecore-bootstrap-decisoes.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md          # §Critério de aceitação, §Comandos canônicos
  - Docs/Roadmap/Tasks/03-core-logging-time-profile.md # apenas §Contratos (VLOG_INFO, SetLogCaptureForTesting, LogLevel)
  - Docs/Roadmap/Tasks/09-jobsystem-worker-queue.md    # apenas §Contratos (JobSystem::Schedule(fn,JobFence&), JobFence, WorkerCount)
  - Docs/Roadmap/Tasks/12-filesystem-async-watcher.md  # apenas §Contratos (FileSystem::ReadAsync, AsyncReadRequest)
  - Docs/Roadmap/Tasks/13-engine-core-bootstrap.md     # apenas §Contratos (EngineCore::Initialize/Shutdown, EngineCoreConfig)
files_create:
  - Engine/Source/Runtime/EngineCore/Tests/EngineCore_Smoke.cpp
  - Scripts/run-tests.ps1
files_modify: []
---

# 14 — VibeTests.exe: smoke global da Fase 1

## Objetivo
Capstone da Fase 1 (design-mvp.md §9): o smoke `[smoke][fase1]` que reproduz literalmente o critério de
aceitação — `EngineCore::Initialize` com 8 workers, agenda trabalho no pool, lê um arquivo de 4 KiB via
`FileSystem::ReadAsync`, loga 32 linhas, `Shutdown` com zero leaks — provando todos os subsistemas operando
coordenadamente. Mais `Scripts/run-tests.ps1` (wrapper de ctest que devolve exit code — ADR 0005). O executor
NÃO precisa ler o design — tudo está aqui.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso**
(HARDENING §14). T12/T13 ainda não estão implementadas na criação → o pre-flight compara contra os `§Contratos`
upstream; ao executar, o gate de dependências garante os headers reais.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §5, §7, §12, §14 |
| Decisions/0005 | smoke `[smoke][fase1]` único; `run-tests.ps1` devolve exit code; hook de captura | inteiro |
| Decisions/0021 | `EngineCore` forma/uso; string `"TrackingAllocator: <N> leaks"`; smoke usa WorkerCount=8 explícito | inteiro |
| Phases/Fase-01 | critério de aceitação da fase + comandos | §Critério de aceitação, §Comandos canônicos |
| Tasks/03 | `VLOG_INFO`, `SetLogCaptureForTesting`, `LogLevel` | §Contratos |
| Tasks/09 | `JobSystem::Schedule(Fn&&, JobFence&)`, `JobFence::Wait`, `WorkerCount` | §Contratos |
| Tasks/12 | `FileSystem::ReadAsync(const Path&) -> AsyncReadRequest` (`Wait`/`Result`) | §Contratos |
| Tasks/13 | `EngineCore::Initialize(const EngineCoreConfig&)`/`Shutdown`, `EngineCoreConfig` | §Contratos |

## Escopo

### Dentro
- 1 arquivo de teste (`EngineCore_Smoke.cpp`) com o `TEST_CASE` `EngineCore_Smoke_Fase1` (`[smoke][fase1]`).
- `Scripts/run-tests.ps1`: roda `ctest --preset debug --output-on-failure` (+ development) e devolve o exit code.

### Fora (NÃO fazer, mesmo que pareça óbvio)
- Qualquer novo símbolo público / header (o smoke só CONSUME a API das tasks anteriores).
- Alterar CMake: o arquivo de teste é auto-globado (ADR 0016; `Vibe::EngineCore` já linkado pela T13) e
  `run-tests.ps1` é standalone — **nenhum `files_modify`**.
- `ParallelFor`/`TaskGraph` (T10) — o smoke usa `JobSystem::Schedule(fn, fence)` (T09) para o batch.
- Medir tempo dentro do teste (o teto < 5 s é do runner/ctest, não asserção wall-clock — §7).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create` exaustivos; `files_modify` é vazio. Exceções permanentes: este doc, README, Backlog. Precisar de
qualquer outro arquivo (ex.: o glob de testes NÃO cobrir `EngineCore/Tests/` ou `Vibe::EngineCore` não linkado
em VibeTests → drift da T13) → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE quando: (a) a API consumida não compila vs `<EngineCore/...>`/`<FileSystem/...>`/`<JobSystem/...>`/`<Core/...>`
reais; (b) o smoke precisar de arquivo fora das listas (ex.: editar CMake → drift do auto-glob/link da T13);
(c) seções se contradizem; (d) o teste afirma comportamento impossível; (e) gate falha após 2 ciclos de `diagnose`.
Relate: **passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (esta task não expõe API — entrega um teste de integração + um script)

Não há header público. Os artefatos:

### Engine/Source/Runtime/EngineCore/Tests/EngineCore_Smoke.cpp
`TEST_CASE("EngineCore_Smoke_Fase1", "[smoke][fase1]")` — usa apenas a superfície pública das tasks anteriores +
`SetLogCaptureForTesting` (`#if VIBE_TESTING`, T03). Auto-globado para `VibeTests.exe` (ADR 0016).

### Scripts/run-tests.ps1
Wrapper PowerShell de `ctest` que devolve `$LASTEXITCODE` (ADR 0005) — consumido por `vx-task-execute`.

**Notas de implementação (decididas na criação; não re-derivar):**
- **Estrutura do smoke** (ADR 0021): (1) instala `SetLogCaptureForTesting(sink)` que empurra `{level,msg}` para
  um `std::vector` file-local; (2) `EngineCore::Initialize({.m_WorkerCount=8})` → `REQUIRE(has_value())`;
  `REQUIRE(JobSystem::WorkerCount()==8)`; (3) agenda `N=64` jobs numa `JobFence` (`JobSystem::Schedule(fn, fence)`,
  T09), cada um `fetch_add(1)` num `std::atomic<int>`; `fence.Wait()`; `REQUIRE(counter==64)` (pool operando);
  (4) escreve um arquivo temp de **4096 bytes** (padrão determinístico, ex. `Vbyte(i & 0xFF)`) via `std::ofstream`;
  `auto req = FileSystem::ReadAsync(Path(temp)); req.Wait();` `auto r = req.Result();` `REQUIRE(r && r->size()==4096)`
  e bytes batem; (5) emite **exatamente 32** `VLOG_INFO` com um prefixo reconhecível (ex. `"Fase1Smoke line ..."`);
  (6) `EngineCore::Shutdown()`; (7) asserções finais sobre a captura — ver abaixo; (8) remove `SetLogCaptureForTesting(nullptr)`
  e apaga o temp (RAII/teardown).
- **Contagem de 32 linhas determinística**: conta na captura as linhas cujo conteúdo casa o prefixo do passo (5)
  → `REQUIRE(==32)`. Contar por prefixo é robusto aos marcadores `"EngineCore: <Passo>"` (T13) e a logs de
  subsistema intercalados (não conta "exatamente 32 no total"). Sem dependência de ordem entre threads (o batch
  já completou via `fence.Wait()` antes do passo 5).
- **Zero-leak** (gate da fase, §12): `EngineCore::Shutdown` emite `"TrackingAllocator: 0 leaks"` no passo Memory
  (T13/ADR 0021); o smoke faz `REQUIRE` de que a captura contém essa linha EXATA. String é contrato com a T13
  (mudou lá → quebra aqui de forma visível).
- **Determinismo** (§7): toda sincronização por `fence.Wait()`/`AsyncReadRequest::Wait()` — sem `sleep_for`, sem
  wall-clock em asserção; `WorkerCount=8` explícito (não toca o gate de HW da ADR 0007, roda em qualquer máquina);
  temp único apagado no teardown; conteúdo de 4 KiB literal.
- **`run-tests.ps1`**: roda `ctest --preset debug --output-on-failure`; se exit 0, roda `ctest --preset development
  --output-on-failure`; `exit $LASTEXITCODE`. Aceita um filtro opcional `-R` repassado. Sem lógica de teste própria
  (verificado por ser o comando que o gate invoca, não por um `TEST_CASE`).
- **Sem CMake**: `EngineCore/Tests/*.cpp` é auto-globado (ADR 0016) e `Vibe::EngineCore`/`Vibe::FileSystem`/
  `Vibe::JobSystem` já estão linkados em `VibeTests` pelas tasks T13/T11/T09 — por isso `files_modify` é vazio.

## Plano de testes (lista RED — ordem de execução do `tdd`)

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `EngineCore_Smoke_Fase1` | `[smoke][fase1]` | EngineCore_Smoke.cpp | `Initialize({m_WorkerCount=8})` ok; `WorkerCount()==8`; 64 jobs numa fence → contador==64; `ReadAsync` de arquivo 4096 B → `Result()` ok, `size()==4096` e bytes batem; 32 linhas de log (prefixo) capturadas == 32; `Shutdown`; captura contém `"TrackingAllocator: 0 leaks"`; < 5 s |

**Smoke desta task**: o próprio teste #1 É o smoke global da fase (reproduz §9 Fase 1 — ADR 0005). Orçamento
< 30 s (alvo < 5 s — §Ordem interna passo 9). **Determinismo**: barreiras do SUT (`fence.Wait`/`req.Wait`),
sem timing/sleep/RNG. **Cobertura**: exercita EngineCore + JobSystem + FileSystem + Logging fim-a-fim (o smoke é
a verificação de integração; a cobertura unitária mora nas tasks T02–T13).

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos)

```powershell
# Configurar + build (gate: /W4 /WX falham com warning novo)
cmake --preset debug; cmake --preset development
cmake --build --preset debug; cmake --build --preset development
# Smoke global da fase (com medição de duração)
Build\debug\bin\VibeTests.exe "[smoke][fase1]" --durations yes
# Suite completa (gate final — todos os módulos T02–T13)
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
# Wrapper entregue por esta task (deve devolver exit code 0 com a suite verde)
pwsh Scripts/run-tests.ps1
```

## Critério de aceitação
- [ ] `EngineCore_Smoke_Fase1` verde — `Build\debug\bin\VibeTests.exe "[smoke][fase1]"`
- [ ] Smoke < 30 s (alvo < 5 s) — saída de `--durations yes`
- [ ] Saída do smoke evidencia: `WorkerCount()==8`, leitura async de 4 KiB OK, 32 linhas logadas, `"TrackingAllocator: 0 leaks"`
- [ ] Suite completa verde em debug e development (≥ 30 testes — critério da fase) — `ctest --preset debug/development`
- [ ] `Scripts/run-tests.ps1` roda a suite e devolve exit code 0 com tudo verde (≠ 0 se algum teste falhar) — `pwsh Scripts/run-tests.ps1`
- [ ] `EngineCore_Smoke.cpp` não cria header/símbolo público; sem `files_modify` (auto-glob + links da T13) — inspeção
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Esta task não expõe símbolos C++. Entrega o smoke da fase e `run-tests.ps1` (reusado pelo `vx-task-execute` das
fases seguintes). Fecha a Fase 01.

| Símbolo/artefato | Consumido por |
|---|---|
| `Scripts/run-tests.ps1` | `vx-task-execute` (gate de validação das fases 2+) |

## Hardening aplicável (referência + concretização)
- §5 → aqui: não há declaração pública nova; Doxygen N/A (é um TU de teste + script).
- §7 → aqui: o smoke É a aplicação — determinístico, barreiras do SUT, < 30 s, fixture temp isolado.
- §12 → aqui: o gate de zero-leak da fase é a asserção da linha `"TrackingAllocator: 0 leaks"` no Shutdown.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
Formato → orçamento → deps (T12/T13 = Implementado) → pre-flight (`EngineCore`/`ReadAsync`/`Schedule`/hook de log
vs Tasks/13/12/09/03) → Em-execução → baseline (suite das tasks anteriores verde) → branch
`task/14-vibetests-exe-smoke-fase1` → `tdd` sobre o smoke → gate completo (suite + smoke + run-tests.ps1) →
Implementado → commit `[task 14] vibetests: smoke global da Fase 1`, push, PR. **Fecha a Fase 01.**

## Desvios aprovados
(vazio na criação)

## Bloqueio
(vazio)
## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §9 · Phases/Fase-01-foundation.md · Decisions/0005, 0021
