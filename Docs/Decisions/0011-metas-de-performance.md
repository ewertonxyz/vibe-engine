---
id: 0011
titulo: Metas de performance — gate 1080p@60 e teto de design 4K@60 via FSR
data: 2026-06-11
status: Aceita
relacionada: [Decisions/0010, Decisions/0013]
---

## Contexto

O critério de aceitação do MVP (design-mvp.md §3 #2) fixa **1080p @ 60 fps estáveis em RTX 3060 / RX 6600**. O usuário definiu como objetivo da engine que os jogos produzidos rodem "no mínimo a 60 fps em 4K". 4K@60 **nativo** em GPU classe 3060 é irreal; 4K@60 com upscaling FSR é viável, mas o custo do próprio FSR em saída 4K (~2–3 ms) aperta o orçamento. É preciso reconciliar as duas metas sem inflar o gate do MVP a ponto de inviabilizá-lo, e fixar os orçamentos de frame que as skills (`vx-spec-memory-perf`, `vx-spec-rendering`) usam para bloquear decisões ruins.

## Opções consideradas

- **A**: Manter apenas 1080p@60 (ignora o objetivo 4K).
- **B**: Mudar o gate do MVP para 4K@60 com FSR Performance na própria RTX 3060 (risco real de o MVP nunca fechar).
- **C**: Gate do MVP permanece 1080p@60 na RTX 3060/RX 6600; **4K@60 via FSR Quality/Balanced em GPU classe RTX 4070+** vira **teto de design** (meta arquitetural verificável, não critério de aceitação do MVP).

## Decisão

**Opção C.**

### Gate (critério de aceitação — inalterado)
`VibeGame.exe` a 1080p @ 60 fps estáveis em RTX 3060 / RX 6600 com a cena completa do MVP (§3 #2).

### Teto de design (meta arquitetural)
A arquitetura do renderer e dos sistemas per-frame deve **sustentar 4K @ 60 fps com FSR Quality/Balanced em GPU classe RTX 4070 / RX 7800 XT** sem mudanças estruturais. Regras derivadas:

1. Nenhum passe pode ter custo proporcional à resolução de **saída** além do estritamente necessário (pós-FSR só tonemap/UI; tudo que puder roda em resolução interna).
2. Motion vectors, jitter e reactive mask corretos desde a integração do TAA/FSR (Fase 4) — são pré-requisito do teto, não polish.
3. Estruturas per-frame dimensionadas para o pior caso do teto (ex.: buffers de luz/cluster em 4K), não apenas para o gate.
4. `vx-spec-rendering` e `vx-spec-memory-perf` **bloqueiam** decisões que inviabilizem o teto, mesmo que passem no gate.

### Orçamento de frame (16.6 ms @ gate)

| Fatia | Orçamento inicial | Observação |
|---|---|---|
| CPU game (sim, gameplay, physics 1/60 fixo) | ≤ 4 ms | physics em job pool dedicado |
| CPU render-prep (visibility, RenderGraph build, submit) | ≤ 2 ms | paralelizado via TaskGraph |
| GPU (GBuffer + shadows + lighting + post + UI) | ≤ 10 ms | inclui HUD RmlUi (< 0.5 ms) |
| Slack / OS / present | ~0.5 ms | margem anti-spike |

São **orçamentos de partida** — refinados por medição Tracy/PIX, nunca citados como fixos sem confirmar com captura. No teto 4K, a fatia GPU absorve adicionalmente o custo do FSR (~2–3 ms), o que reforça a regra 1.

## Consequências

- HARDENING ganha §13 (orçamento de performance): Tracy zones obrigatórias em todo sistema per-frame; teste `[perf]` obrigatório quando a task tiver `risco_frame: true`; tabela acima como referência.
- design-mvp.md: §3 ganha bloco "Meta de design (não-gate)"; nova seção de pilares de performance/memória após §3.
- `vx-spec-memory-perf` e `vx-spec-rendering` citam esta ADR e aplicam a regra de bloqueio do teto.
- O gate continua sendo o único critério que fecha o MVP — o teto nunca atrasa a aceitação, apenas veta arquitetura que o impossibilite.
