---
id: 0005
titulo: Infraestrutura de testes — VibeTests.exe único, FileWatcher.Poll
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation]
---

## Contexto

HARDENING §7 exige Catch2 com "tags por módulo para execução seletiva". O design não fixa se isso se manifesta como um único executável de testes ou um por módulo. Há dois pontos correlatos:

1. Granularidade do binário de testes.
2. `FileWatcher` (entregue na Fase 1) é assíncrono por natureza (`ReadDirectoryChangesW` no Windows). Teste determinístico sem dormir exige uma API extra.

## Opções consideradas

- **Executável de testes**: (A) `VibeTests.exe` único com tags; (B) um exe por módulo.
- **FileWatcher determinístico**: (A) expor `Poll()` síncrono em build de teste; (B) testes usam `WaitForMultipleObjects` com timeout de 2 s.

## Decisão

- **Um único `VibeTests.exe`** com tags por módulo: `[core]`, `[platform]`, `[math]`, `[memory]`, `[job]`, `[filesystem]`, plus tier tags `[smoke]`, `[unit]`, `[integration]`, e tag de fase `[fase1]`. Razão: HARDENING §7 menciona "tags por módulo para execução seletiva" — pressupõe um runner único; link único; fixtures compartilhados; smoke global trivial.
- **`FileWatcher::Poll()` síncrono exposto em build de teste** (não em Shipping). Em produção, o watcher mantém o modelo callback assíncrono. Em build de teste, `Poll()` drena eventos pendentes sem dormir, eliminando flakiness por timing.

## Consequências

- CMake define o alvo `VibeTests` agregando `target_sources` de cada `Engine/Source/Runtime/<Module>/Tests/`. `catch_discover_tests()` popula CTest.
- Preset `test-debug` e `test-development` rodam `ctest --output-on-failure -j`. Sem preset de Shipping (testes não rodam em Shipping).
- Smoke da Fase 1 é único `TEST_CASE` `[smoke][fase1]` reproduzindo literalmente o critério §9 Fase 1 ("8 workers + read async + log").
- `FileWatcher` header em `Public/` tem `#if defined(VIBE_TESTING)` guard ao redor de `Poll()`. Flag definida apenas pelo alvo `VibeTests` em CMake. Zero impacto em runtime de produção.
- Script `Scripts/run-tests.ps1` envoltório de CTest devolve exit code para `vx-task-execute` consumir.
