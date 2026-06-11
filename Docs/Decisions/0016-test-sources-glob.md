---
id: 0016
titulo: Agregação de fontes de teste do VibeTests por glob
data: 2026-06-11
status: Aceita
relacionada: [Decisions/0005, Tasks/02-core-types-handles-result, Tasks/03-core-logging-time-profile]
---

## Contexto

O alvo único de testes `VibeTests` (ADR 0005) é definido em `Engine/Source/Tests/CMakeLists.txt`.
Na Task 02 ele foi implementado com **lista explícita** dos 6 arquivos de teste do Core. A
Task 03 adiciona 2 novos arquivos de teste (`Core_Logging.cpp`, `Core_Time.cpp`), mas seu
`files_modify` lista apenas `Engine/Source/Runtime/Core/CMakeLists.txt` — não o CMake do alvo de
testes. Com lista explícita, os testes novos nunca seriam compilados/descobertos sem editar um
arquivo fora do escopo declarado da task.

A nota da própria Task 02 descrevia o alvo "sobre `Core/Tests/*.cpp`" — ou seja, a intenção de
design era um glob. A lista explícita foi um desvio de implementação que só ficou visível na T03.

## Opções consideradas

- **A. Glob com `CONFIGURE_DEPENDS`**: `file(GLOB ... CONFIGURE_DEPENDS Engine/Source/Runtime/*/Tests/*.cpp)`.
  Cada módulo deixa seus testes em `Tests/` e eles entram no alvo automaticamente; tasks futuras
  não tocam o CMake do alvo. `CONFIGURE_DEPENDS` força re-glob a cada build.
- **B. Lista explícita**: cada task que adiciona testes edita `Engine/Source/Tests/CMakeLists.txt`.
  Mais determinístico, mas obriga toda task de teste a incluir esse arquivo no `files_modify`.

## Decisão

**Opção A.** `Engine/Source/Tests/CMakeLists.txt` agrega via
`file(GLOB VibeTestSources CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Engine/Source/Runtime/*/Tests/*.cpp)`.

Decisão tomada pelo usuário durante a execução da Task 03 (corrige o desvio de implementação da
T02 e alinha com a intenção "`*.cpp`" registrada na nota da T02). A edição do arquivo, fora do
`files_modify` da T03, foi autorizada explicitamente e registrada como Desvio aprovado.

## Consequências

- Tasks futuras adicionam testes apenas criando `Engine/Source/Runtime/<Module>/Tests/*.cpp`; o
  alvo `VibeTests` os agrega sem edição de CMake — `files_modify` dessas tasks não precisa incluir
  `Engine/Source/Tests/CMakeLists.txt`.
- `CONFIGURE_DEPENDS` faz o build re-rodar o glob quando arquivos entram/saem; o fluxo do executor
  (que sempre roda `cmake --preset` antes de buildar) já garante a re-detecção.
- Trade-off aceito: glob de fontes é desencorajado por parte da comunidade CMake; aqui o ganho de
  escopo por task supera, e `CONFIGURE_DEPENDS` mitiga o risco de stale.
