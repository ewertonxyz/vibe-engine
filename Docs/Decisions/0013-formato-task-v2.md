---
id: 0013
titulo: Formato de task v2 — spec autossuficiente; criação com modelo grande, execução com modelo menor
data: 2026-06-11
status: Aceita
relacionada: [Decisions/0010, Decisions/0011, Phases/Fase-01-foundation]
---

## Contexto

O fluxo operacional do projeto passa a usar **modelos diferentes por etapa**: fases e tasks são geradas com um modelo grande (classe Opus), e a execução das tasks roda com um modelo de contexto menor (classe Sonnet). A auditoria das skills revelou que a arquitetura v1 era invertida para esse split:

- Tasks deliberadamente curtas (< 60 linhas), com contratos "a produzir" pelos especialistas **em tempo de execução** (vx-task-execute passo 6) — o executor precisava interpretar texto não estruturado de subagentes para extrair assinaturas e planos de teste.
- Nenhum comando exato de build/teste em lugar algum; smoke test sem definição; gate de Doxygen sem comando.
- `files_touch` era "previsão" na criação, mas vinculante na execução — contradição que bloquearia o executor a cada arquivo novo.
- Perguntas levantadas por especialistas dentro de subagentes (que não têm `AskUserQuestion`) podiam morrer sem chegar ao usuário.

## Opções consideradas

- **A**: Manter o formato v1 e confiar que o executor reconsulta especialistas em runtime.
- **B**: **Formato v2** — inverter o fluxo: todo o pensamento acontece na criação; o doc da task é uma spec autossuficiente; a execução é quase-mecânica.

## Decisão

**Opção B — formato v2.** O doc de task é um **artefato compilado**: o criador compila design-mvp + fase + especialistas + decisões do usuário numa spec fechada; o executor a executa e reporta uma falha estruturada quando a spec não bate com a realidade.

### Frontmatter v2
`formato: 2` (gate de execução — doc sem ele é v1 e exige rebake) · `especialistas_consultados:` (proveniência; substitui `specialists:` — especialistas NÃO são chamados na execução) · `contexto:` (lista exata e exaustiva de leitura do executor, ≤ 8 arquivos) · `files_create:` / `files_modify:` (vinculantes; substituem `files_touch`) · `risco_memoria:` / `risco_frame:` (disparam gate asan-debug e teste `[perf]`, HARDENING §12/§13).

### Seções obrigatórias do corpo
`Objetivo` · `Contexto obrigatório (orçamento de leitura)` · `Escopo` (Dentro/Fora/semântica vinculante/protocolo de bloqueio) · `Contratos` (declarações C++ públicas verbatim + notas de implementação) · `Plano de testes` (tabela com nomes exatos de TEST_CASE, tags, arquivos, asserções — é a lista RED do `tdd`) · `Comandos` (bloco copiável, fonte: "Comandos canônicos da fase") · `Critério de aceitação` (cada item mapeado a comando/evidência) · `Interface para tasks sucessoras` · `Hardening aplicável` (referência + concretização, nunca duplicação) · `Fluxo de execução` · `Desvios aprovados` · `Bloqueio` · `Referências`.

### Tetos (estourou → split na criação)
Doc 150–250 linhas (teto 300) · Contratos ≤ 100 linhas C++; ≤ 3 headers públicos **não-triviais** e ≤ 12 símbolos públicos **não-triviais** (headers/famílias alias-only contam como 1 grupo) · Plano de testes ≤ 15 casos · `contexto:` ≤ 8 arquivos.

### Regras de fluxo
1. Especialistas são consultados **na criação** (subagentes paralelos com schema de output obrigatório, incluindo seção `OPEN-QUESTIONS` que o criador transforma em `AskUserQuestion` antes de escrever o doc).
2. Fase docs ganham as seções `Contratos entre tasks`, `Comandos canônicos da fase`, `Critério de aceitação — verificação por máquina`, `Flags de risco por task` e `Protocolo de revisão de contratos` — fontes únicas de onde as tasks copiam.
3. Execução: gate de formato → orçamento de leitura (task + `contexto:` + `files_modify:`, **nada mais** — fato faltante é bloqueio, não busca) → pre-flight de contratos contra headers reais → TDD sobre a lista RED → gate de validação com comandos verbatim.
4. Drift: desvio aprovado que muda símbolo exposto → emenda primeiro o registro da fase → rebake das tasks `Planejado` afetadas; tasks `Implementado` ganham task corretiva, nunca edição retroativa.
5. Norma operacional: criar tasks **just-in-time** (1–3 à frente da execução), não a fase inteira de uma vez — minimiza rebake por drift.

## Consequências

- HARDENING ganha §14 (contrato de execução de tasks). `vx-task-create`, `vx-task-execute` e `vx-phase-analyze` são reescritas; especialistas ganham schema de output; `vx-dependency-check`/`vx-doc-graph` leem os campos novos.
- Tasks v1 existentes (01–03) são reescritas no formato v2 antes de qualquer execução.
- Tasks ficam maiores (150–250 linhas), e isso é intencional: o custo é pago uma vez na criação (modelo grande), economizando re-derivação a cada execução (modelo menor).
- `Docs/Roadmap/README.md` e `Docs/References/VIBE-CODING-WORKFLOW.md` documentam o split de modelos.
