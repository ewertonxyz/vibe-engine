---
id: 0017
titulo: Convenções do módulo Math — INTERFACE library, typedefs no root, operações como free functions
data: 2026-06-11
status: Aceita
relacionada: [Phases/Fase-01-foundation, Tasks/04-math-wrappers-glm, Decisions/0014]
---

## Contexto

A Task 04 entrega o módulo `Math` como wrappers finos sobre glm (design-mvp.md §8.3). Três
escolhas estruturais precisam ser fixadas antes de assar a task, pois definem como ~25 módulos
das fases seguintes escreverão tipos e operações matemáticas:

1. O alvo CMake do `Math` é INTERFACE (header-only) ou STATIC (com TU próprio)?
2. Onde vivem os typedefs `Vec2/Vec3/Vec4/Mat4/Quat` e a struct `Transform` — no namespace raiz
   `VibeEngine` ou em `VibeEngine::Math`?
3. As operações são operadores-membro do glm reexpostos, ou free functions próprias?

A fase já levantou o risco "glm exposto em headers Public — troca futura cara" e a mitigação
"typedefs com prefixo V; operações via funções livres em `VibeEngine::Math`".

## Opções consideradas

- **Alvo**: (A) INTERFACE header-only; (B) STATIC com `Private/Math.cpp`.
- **Namespace dos tipos**: (A) raiz `VibeEngine`; (B) `VibeEngine::Math`.
- **Operações**: (A) free functions em `VibeEngine::Math`; (B) usar diretamente os operadores glm.

## Decisão

- **Alvo INTERFACE.** O mapa de módulos da fase não lista nenhum `.cpp` para Math; todos os
  símbolos são wrappers triviais `inline`. `add_library(VibeMath INTERFACE)` + alias `Vibe::Math`;
  `target_include_directories(VibeMath INTERFACE Public)`; `target_link_libraries(VibeMath INTERFACE
  glm::glm Vibe::Core)`.
- **Tipos no namespace raiz `VibeEngine`** (`Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quat`, `Transform`),
  como os demais tipos fundamentais de §8.1. NÃO recebem prefixo `V` — não são escalares
  fundamentais (HARDENING §3 reserva o prefixo `V` para `Vint*`/`Vspan`/`VHandle`/etc.); são tipos
  PascalCase de valor.
- **Operações como free functions em `VibeEngine::Math`** (ex.: `Math::Identity()`,
  `Math::Normalize(const Quat&)`, `Math::Lerp(const Vec3&, const Vec3&, Vfloat)`). A engine chama
  `Math::Fn(...)`, não os operadores-membro do glm, para que uma troca futura de backend toque só
  o módulo Math.
- **glm exposto em headers Public** (`#include <glm/glm.hpp>` em `MathCommon.h`) é o trade-off
  aceito pela fase. Os typedefs + free functions confinam a *superfície de chamada* da engine.

## Consequências

- `Transform` é uma struct própria `{ Vec3 m_Position; Quat m_Rotation; Vec3 m_Scale; }` (glm não
  tem equivalente); campos com prefixo `m_` (HARDENING §3; precedente `EngineCoreConfig`, ADR 0006).
- Superfície pública mínima no MVP: typedefs + apenas as operações que os testes da T04 exercem
  (`Identity`, `Normalize`/`Length` de Quat, `Lerp` de Vec3). Operações adicionais (Dot, Cross,
  Inverse, LookAt, Slerp...) entram **just-in-time** quando Renderer/Camera consumirem Math — cada
  símbolo público novo carrega seu teste (HARDENING §7).
- Numa troca pós-MVP de glm: mudam os typedefs e os corpos das free functions num só módulo;
  call-sites que respeitaram `Math::Fn` permanecem. A convenção não é compilável-forçada (erosão é
  pega em review / `vx-naming-style`).
- Por ser INTERFACE, `Math` não compila um TU próprio; erros de uso aparecem nos consumidores.
