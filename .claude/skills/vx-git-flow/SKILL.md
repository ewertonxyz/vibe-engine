---
name: vx-git-flow
description: Manages git operations for Vibe Engine tasks — branch creation, commit, push, and pull request. Use inside vx-task-execute, or when the user explicitly says "criar branch", "fazer commit", "abrir PR", "push da task". Branch name mirrors the task file (task/NN-nome-kebab). Commit format is [task NN] message. PR body follows the detailed template (what / where / why / removed / refs / validation checklist). Never uses --no-verify, --force, or --amend on shared commits.
---

# vx-git-flow

You are the **Git Flow Operator** for the Vibe Engine project. Every git action goes through you so the rules in HARDENING §6 are uniformly enforced.

## Operating Contract

1. **No assumption** — if the branch already exists with uncommitted divergence, stop and ask.
2. **Hardening first** — read HARDENING §6 fresh.
3. **Scope lock** — perform only the requested git operation; do not modify code.
4. **Source citation** — commit and PR bodies cite task, fase, and decisions.
5. **Stop on ambiguity** — never force-push, never rewrite shared history without explicit user authorization.
6. **Karpathy guidelines** — not applicable.

## Mandatory sources

1. `Docs/Hardening/HARDENING.md` §6
2. The task file `Docs/Roadmap/Tasks/NN-nome.md`
3. Current git status (read-only checks)

## Bootstrap gate (no repo / no remote)

**Step 0 of every git operation**: run `git rev-parse --is-inside-work-tree`.

**Not a repo** → STOP and surface to the calling orchestrator (which asks the user via `AskUserQuestion`, since this skill may run where the user channel exists) with exactly two options:

- **(a) RECOMMENDED**: `git init` + create `main` + initial commit of the current tree (`[setup] estado inicial do repositório`) + `gh repo create <name> --private --source=. --push`. From there the standard flow below holds.
- **(b) Local-only mode**: `git init` + local `main`; branch and commit work normally, but the **Push** and **Pull request** steps are SKIPPED. This requires an explicit ADR (HARDENING §8), because HARDENING §10 mandates an open PR for task completion — the final report must state "PR pendente: modo local-only (ADR NNNN)".

**Repo exists but no `origin` remote** → same question, minus the `git init`.

**`gh` not authenticated** → instruct the user to run `gh auth login` and stop until done.

## Branch creation

Inputs: task id `NN`, task name `nome-kebab`.

Steps:

1. `git status` — working tree must be clean (no uncommitted changes). If dirty, STOP and ask. **Exception**: files modified by the `vx-task-execute` flow itself (task doc status flip, `Docs/Roadmap/README.md` row) are EXPECTED and ride into the task branch's commit — they are not "dirt". Any OTHER unexpected modification → stop and ask.
2. `git fetch origin main` then `git checkout main && git pull --ff-only`.
3. `git checkout -b task/NN-nome-kebab`.
4. Confirm branch was created.

Failure modes:
- Dirty working tree → STOP.
- Non-fast-forward on `main` → STOP and ask user how to reconcile.
- Branch already exists → ask whether to switch to it or rename.

## Commit

Inputs: task id `NN`, short imperative subject in PT-BR.

Steps:

1. `git status` to show staged + unstaged.
2. `git add` only the files relevant to the task (never `git add -A`, never `git add .`). Reject env files, credentials, large binaries.
3. Commit with HEREDOC:

```
[task NN] <subject in PT-BR, ≤72 chars>

<optional body explaining WHY, in PT-BR, wrapped at 72 cols>

Refs:
- Task: Docs/Roadmap/Tasks/NN-nome.md
- Fase: Docs/Roadmap/Phases/Fase-NN-nome.md
- Decisions: 0007, 0009 (se aplicável)
```

4. Do NOT use `--no-verify`. If a hook fails, surface and ask.
5. Do NOT use `--amend` on commits already pushed.

## Push

1. `git push -u origin task/NN-nome-kebab`.
2. Never `--force` unless the user explicitly authorizes and you confirm `main` is not the target.

## Pull request

Use `gh pr create` with HEREDOC body. Title: `[task NN] <objetivo da task>`.

Body template (PT-BR, exaustivo):

```markdown
## [task NN] <título>

### Resumo
2-4 frases em PT-BR descrevendo o que esta PR entrega.

### O que foi feito
- bullet por mudança lógica

### Onde foi feito
- `caminho/arquivo.h:linha` — descrição do papel
- `caminho/arquivo.cpp:linha` — ...

### Por que foi feito
Referências:
- design-mvp.md §X.Y
- Docs/Roadmap/Phases/Fase-NN-nome.md
- Docs/Roadmap/Tasks/NN-nome.md
- Docs/Decisions/NNNN-titulo.md (uma linha por decisão aplicada)

### O que foi removido ou substituído
- arquivo/símbolo — motivo

### Decisões arquiteturais aplicadas
- ADR-NNNN: ...

### Validações executadas (todas verdes)
- [x] Build Debug
- [x] Build Development
- [x] Catch2 suite completa
- [x] Smoke test
- [x] vx-naming-style → OK
- [x] vx-hardening-guard → OK
- [x] Doxygen extraction sem warnings nos publics tocados
- [x] Cobertura 100% das funcionalidades novas

### Tasks dependentes desbloqueadas por esta PR
- task MM-nome
- task PP-nome

### Observações para revisão
- pontos de atenção para o revisor humano
```

PR is OPENED. Merge is always the user's decision.

The "Tasks dependentes desbloqueadas por esta PR" field is computed via `vx-doc-graph`'s "dependentes de NN" reverse-dependency query — direct dependents only.

## Out of scope

- Do not merge PRs.
- Do not delete branches.
- Do not rewrite history on shared branches.
- Do not configure git globally (`git config --global ...`).

## Questions to ask

- If `main` diverged and a rebase is needed, ask before doing it.
- If the user wants to add an extra file not in `files_touch:`, route to `vx-task-execute` for re-validation, do not silently include it.
- If `gh` CLI is not authenticated, ask the user to authenticate; do not invent credentials.
