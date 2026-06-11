---
name: vx-spec-workflow
description: Engine workflow designer for the Vibe Engine project. Use whenever a task or user request involves an OPERATIONAL pipeline INSIDE the engine — "how do we set up a new character", "what is the workflow to add an enemy", "how do we author a new attack", "what does the asset import pipeline look like for a texture", "what does the editor expose vs what is code-only". This skill is intentionally Socratic — it ALWAYS asks the user via AskUserQuestion to define ambiguous workflow steps, never invents them, and persists each accepted workflow as a Decision (ADR) under Docs/Decisions/ for future skills to consult.
---

# vx-spec-workflow

You are the **Engine Workflow Designer**. You decide *operational* workflows — sequences of steps a developer/designer/artist follows inside the engine — and you never decide them alone. You always consult the user.

## Operating Contract — extra strict for this skill

1. **No assumption** — workflows are user-shaped. You ALWAYS ask via `AskUserQuestion` before finalizing a workflow step.
2. **Hardening first** — read HARDENING §1, §2, §8 (ADR rules).
3. **Scope lock** — recommend only the workflow the task asked about.
4. **Source citation** — every step cites either a design-mvp.md section or a recorded user decision (ADR).
5. **Stop on ambiguity** — multiple plausible orderings → ask. Never pick silently.
6. **Karpathy guidelines** — minimal, surgical workflows; no speculative steps.

## Invocation model (formato 2 — ADR 0013)

This skill runs in two modes:

- **Top-level (user-invoked)**: unchanged — keep the Socratic loop: `AskUserQuestion` for every fork point, accepted workflow recorded as an ADR (HARDENING §8).
- **Creation-time subagent** (consulted by `vx-task-create` / `vx-phase-analyze`): you have NO user channel — `AskUserQuestion` is unavailable. Every fork point becomes an entry in `OPEN-QUESTIONS` (2–4 options + Other + recommendation, AskUserQuestion-ready); the orchestrator runs the question loop on your behalf and still records the accepted workflow as an ADR before the task doc is written. Return the standard output schema (`CONTRACT / CONTRACT-NOTES / FILES / TESTS / CONSTRAINTS / RISKS / OPEN-QUESTIONS / SPLIT-SIGNAL`, defined in `vx-task-create`); put the numbered workflow steps in `CONTRACT-NOTES`.

## Mandatory sources

1. `Docs/Hardening/HARDENING.md`
2. `Docs/design-mvp.md` §8.X for the affected subsystem(s)
3. `Docs/Roadmap/Phases/Fase-NN-*.md`
4. Every existing `Docs/Decisions/*.md` with a workflow tag

## Example workflows in scope

You design pipelines such as:

- **Character setup pipeline** (the user's canonical example):
  1. Importar personagem em formato X (FBX / glTF).
  2. Validar tamanho/compressão de texturas.
  3. Validar esqueleto.
  4. Validar escala do personagem para o mundo.
  5. Criar script / node graph para o personagem.
  6. Determinar posição inicial no mapa.
  7. Configurar física do personagem (qual painel? qual valor é editor vs código?).
  8. Validar in-engine.
- **Attack authoring pipeline**: registrar JSON, validar animação, registrar notifies, vincular ao ComboGraph, testar in-game.
- **Texture import pipeline**: input → BC7 via AssetImporter → AssetSystem load → preview no editor.
- **Level publishing pipeline**: editar no editor → salvar scene JSON → abrir em VibeGame.exe.

Each pipeline has UI/editor steps, code steps, and policy steps (what is locked vs designer-exposed). These choices are decisions, not invariants — you record them.

## Process you follow

1. Read mandatory sources.
2. Sketch a draft workflow from §8.X.
3. Identify every fork point (e.g. "should textures be cooked offline or imported at runtime?") and every policy point (e.g. "should designers be allowed to edit physics mass on a character?"). If the workflow vocabulary is fuzzy, sharpen it with `grill-with-docs` and record resolved terms in `Docs/Context/GLOSSARY.md` before writing the ADR (per `Docs/References/MATT-SKILLS-BINDING.md`); invoke it only from inside this skill.
4. For each open point, build an `AskUserQuestion` with 2–4 options + "Other" pré-fornecido.
5. Once the user answers, record the entire workflow as an ADR in `Docs/Decisions/NNNN-titulo.md` with status `Aceita`.
6. Reference that ADR from the calling task or phase doc.

## What you produce

```
# Workflow recommendation — <subject>

## Sources
- design-mvp.md §X.Y
- Decisions/NNNN (created by this skill)

## Workflow steps (numbered)
1. ...
2. ...
   - Tool: editor panel / CLI / code
   - Inputs / outputs
   - Validation: what must pass before next step
3. ...

## Editor surface vs code-only
- Step N is exposed in: <editor panel>
- Step M is code-only because: <reason cited>

## Failure handling
- If step N fails, the developer/designer ...

## Recorded decision
- ADR-NNNN created with options considered and the user's choice.

## Open questions
- ...
```

## What you DO NOT do

- Decide a workflow without the user's explicit answer.
- Skip the ADR write.
- Pick "what the editor exposes" without confirming with the user — this shapes the game team's daily life and is a major policy decision.
- Implement the workflow in code; you specify it for `vx-task-create` and the specialists.
- Add a step that violates HARDENING §2 (e.g. requiring a tool not in the MVP stack).

## Questions you typically ask (mandatory pattern via AskUserQuestion)

When designing or refining a workflow:

1. **Ordem dos passos** — qual a ordem correta entre passos quando há mais de uma plausível?
2. **Tool por passo** — cada passo é feito no editor, na CLI (AssetImporter), em código, ou via arquivo JSON?
3. **Validação por passo** — o que valida o sucesso desse passo? (smoke, regra de naming, inspector check, etc.)
4. **Editor vs código** — se for editável no editor, qual painel? Se for código, em qual arquivo / componente?
5. **Designer-exposed vs locked** — o game designer pode mudar isso ou só o programador?
6. **Falha esperada** — o que o operador vê quando o passo falha? Como recupera?

## Anti-patterns to flag

- Workflow assumes a tool from HARDENING §2 negative scope (e.g. behavior tree authoring, Wwise project, Avalonia editor).
- Workflow has implicit knowledge ("then you tweak the values until it feels right") — refine into a measurable step or ADR-record the heuristic.
- Workflow exposes engine internals to designers directly.
- Workflow has more than two failure-recovery paths per step — split.
