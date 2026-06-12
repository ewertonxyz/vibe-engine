---
id: 04
nome: math-wrappers-glm
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [02]
decisions: [0014, 0017]
especialistas_consultados: [vx-spec-architecture, vx-spec-testing]
risco_memoria: false
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Decisions/0017-math-module-conventions.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md   # §Contratos entre tasks (T04), §Comandos canônicos
  - Docs/Roadmap/Tasks/01-cmake-vcpkg-presets.md  # apenas §Contratos (convenções de build, glm via vcpkg)
  - Docs/Roadmap/Tasks/02-core-types-handles-result.md  # apenas §Contratos (Vfloat de Types.h)
files_create:
  - Engine/Source/Runtime/Math/CMakeLists.txt
  - Engine/Source/Runtime/Math/Public/Math/MathCommon.h
  - Engine/Source/Runtime/Math/Public/Math/Vec2.h
  - Engine/Source/Runtime/Math/Public/Math/Vec3.h
  - Engine/Source/Runtime/Math/Public/Math/Vec4.h
  - Engine/Source/Runtime/Math/Public/Math/Mat4.h
  - Engine/Source/Runtime/Math/Public/Math/Quat.h
  - Engine/Source/Runtime/Math/Public/Math/Transform.h
  - Engine/Source/Runtime/Math/Tests/Math_Wrappers.cpp
files_modify:
  - Engine/Source/Runtime/CMakeLists.txt
  - Engine/Source/Tests/CMakeLists.txt
---

# 04 — Math: wrappers finos sobre glm

## Objetivo
Entregar o módulo `Math` como wrappers finos sobre glm (design-mvp.md §8.3, ADR 0017): typedefs
`Vec2/Vec3/Vec4/Mat4/Quat` e a struct `Transform` no namespace raiz `VibeEngine`, e as operações
mínimas (`Identity`, `Normalize`/`Length` de Quat, `Lerp` de Vec3) como free functions em
`VibeEngine::Math`. Alvo INTERFACE (header-only). O executor NÃO precisa ler o design — os contratos
abaixo são completos.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além
disso** (HARDENING §14).

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4 (include ADR 0014), §5, §7 |
| Decisions/0014 | include `<Math/Foo.h>` a partir de Public/ | inteiro |
| Decisions/0017 | INTERFACE lib, typedefs no root, free functions, glm exposto | inteiro |
| Phases/Fase-01 | contrato T04 + comandos canônicos | §Contratos entre tasks (T04), §Comandos canônicos |
| Tasks/01 | glm disponível via vcpkg; alvo/teste; bin/ | §Contratos |
| Tasks/02 | `Vfloat` de `<Core/Types.h>` | §Contratos |

## Escopo

### Dentro
- 7 headers Public em `Public/Math/` (ADR 0014), alvo INTERFACE `Vibe::Math`, 1 arquivo de teste (4 casos).
- Superfície MÍNIMA: só os typedefs + as 4 operações que os testes exercem.

### Fora (NÃO fazer, mesmo que pareça óbvio)
- Nenhuma operação além de `Identity`/`Normalize`/`Length`/`Lerp` (Dot, Cross, Inverse, LookAt, Slerp,
  Perspective, conversões grau/rad, operadores livres → just-in-time nas fases que os consumirem).
- Não criar `.cpp` em `Math/` (é INTERFACE; tudo `inline` em header — ADR 0017).
- Não usar os operadores-membro do glm nos call-sites da engine (convenção ADR 0017).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create`/`files_modify` exaustivos. Exceções permanentes: este doc, README do roadmap, Backlog.
Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) `<glm/glm.hpp>` não resolve via vcpkg; (b) `Vfloat` real
da T02 difere do assumido; (c) seções deste doc se contradizem; (d) teste impossível; (e) gate falha
após 2 ciclos de `diagnose`. Relate: **passo · trecho do doc · erro literal · repro mínimo · resolução
sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <Math/Foo.h>`)

### Engine/Source/Runtime/Math/Public/Math/MathCommon.h
```cpp
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <Core/Types.h>   // Vfloat

namespace VibeEngine::Math
{
/// @brief Tolerância padrão para comparações de ponto flutuante e normalização.
inline constexpr Vfloat Epsilon = 1e-6f;
}
```

### Vec2.h / Vec3.h / Vec4.h / Mat4.h / Quat.h / Transform.h
```cpp
// Vec2.h
#include <Math/MathCommon.h>
namespace VibeEngine { /// @brief Vetor 2D (typedef fino de glm — ADR 0017). 
using Vec2 = glm::vec2; }

// Vec3.h
#include <Math/MathCommon.h>
namespace VibeEngine { /// @brief Vetor 3D. 
using Vec3 = glm::vec3; }
namespace VibeEngine::Math {
/// @brief Interpolação linear componente-a-componente. @param A início @param B fim @param T fator @return A+(B-A)*T.
inline Vec3 Lerp(const Vec3& A, const Vec3& B, Vfloat T) { return glm::mix(A, B, T); }
}

// Vec4.h
#include <Math/MathCommon.h>
namespace VibeEngine { /// @brief Vetor 4D. 
using Vec4 = glm::vec4; }

// Mat4.h
#include <Math/MathCommon.h>
namespace VibeEngine { /// @brief Matriz 4x4 coluna-maior. 
using Mat4 = glm::mat4; }
namespace VibeEngine::Math {
/// @brief Matriz identidade 4x4. @return Identidade.
inline Mat4 Identity() { return Mat4(1.0f); }
}

// Quat.h
#include <Math/MathCommon.h>
namespace VibeEngine { /// @brief Quaternion de rotação (w,x,y,z). 
using Quat = glm::quat; }
namespace VibeEngine::Math {
/// @brief Normaliza o quaternion. @param Q entrada @return Q com comprimento 1.
inline Quat Normalize(const Quat& Q) { return glm::normalize(Q); }
/// @brief Comprimento (norma) do quaternion. @param Q entrada @return |Q|.
inline Vfloat Length(const Quat& Q) { return glm::length(Q); }
}

// Transform.h
#include <Math/Vec3.h>
#include <Math/Quat.h>
namespace VibeEngine {
/**
 * @brief Posição/rotação/escala de um objeto (não é tipo glm — ADR 0017).
 */
struct Transform
{
    Vec3 m_Position { 0.0f, 0.0f, 0.0f };   ///< @brief Translação.
    Quat m_Rotation { 1.0f, 0.0f, 0.0f, 0.0f }; ///< @brief Rotação (identidade = w=1).
    Vec3 m_Scale { 1.0f, 1.0f, 1.0f };      ///< @brief Escala por eixo.
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- Alvo **INTERFACE** (ADR 0017): `Math/CMakeLists.txt` faz `add_library(VibeMath INTERFACE)` + alias
  `Vibe::Math`; `target_include_directories(VibeMath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Public)`;
  `find_package(glm CONFIG REQUIRED)`; `target_link_libraries(VibeMath INTERFACE glm::glm Vibe::Core)`.
- `Engine/Source/Runtime/CMakeLists.txt` (files_modify): adicionar `add_subdirectory(Math)`.
- `Engine/Source/Tests/CMakeLists.txt` (files_modify): adicionar `Vibe::Math` ao
  `target_link_libraries(VibeTests PRIVATE ...)` (o glob de fontes já pega `Math/Tests/*.cpp` — ADR 0016 —
  mas o **link** do alvo `Vibe::Math` é explícito).
- `Quat{1,0,0,0}` é a identidade na ordem do **construtor** glm `(w,x,y,z)` — atenção: o *layout em memória*
  do `glm::quat` é `(x,y,z,w)`; nunca indexe `.x/.y/.z/.w` assumindo a ordem do construtor. Os valores dos
  testes (identidade `{1,0,0,0}`, não-unitário `{0,0,0,2}`) são autoconsistentes pela ordem do construtor.
- Tudo `inline` (header-only). Doxygen em toda declaração pública (HARDENING §5).

## Plano de testes (lista RED — ordem de execução do `tdd`)

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Math_Smoke_Task04` | `[math][smoke]` | Math/Tests/Math_Wrappers.cpp | constrói `Transform t{}` e lê `m_Position/m_Rotation/m_Scale`; `Mat4 m=Math::Identity()`; `Math::Length(Math::Normalize(Quat{0,0,0,2})) ≈ 1`; `Math::Lerp({0,0,0},{2,4,6},0.5f) ≈ {1,2,3}`; `STATIC_REQUIRE(Math::Epsilon > 0.0f)`; constrói Vec2/Vec3/Vec4/Mat4/Quat; < 1 s |
| 2 | `Math_Mat4_Identity` | `[math][unit]` | Math/Tests/Math_Wrappers.cpp | `Math::Identity()`: diagonal ≈ 1, off-diagonal ≈ 0; `Identity()*Vec4{x,y,z,w} ≈ {x,y,z,w}` |
| 3 | `Math_Quat_Normalize` | `[math][unit]` | Math/Tests/Math_Wrappers.cpp | `Length(Normalize(q)) ≈ 1` p/ q não-unitário (`{0,0,0,2}`, `{1,1,1,1}`); identidade `{0,0,0,1}` preservada |
| 4 | `Math_Common_Lerp` | `[math][unit]` | Math/Tests/Math_Wrappers.cpp | `Lerp(a,b,0) ≈ a`, `Lerp(a,b,1) ≈ b`, `Lerp(a,b,0.5) ≈ (a+b)/2` |

**Smoke**: teste #1. **Determinismo** (HARDENING §7): entradas literais fixas, sem RNG/wall-clock;
comparações FP via `Catch::Matchers::WithinAbs(esperado, TestEpsilon)` com `constexpr Vfloat
TestEpsilon = 1e-5f` (nunca `==` cru em float). **Cobertura**: cada free function pública
(`Identity`, `Normalize`, `Length`, `Lerp`) e os typedefs aparecem em ≥ 1 linha.

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos)

```powershell
cmake --preset debug
cmake --preset development
cmake --build --preset debug
cmake --build --preset development
ctest --preset debug --output-on-failure -R "Math_"
Build\debug\bin\VibeTests.exe "[math][smoke]" --durations yes
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
```

## Critério de aceitação
- [ ] 4 TEST_CASE do plano verdes — `ctest --preset debug -R "Math_"`
- [ ] Smoke `Math_Smoke_Task04` < 1 s — saída de `--durations yes`
- [ ] Suite completa verde em debug e development (não regrediu)
- [ ] Headers Public em `Public/Math/`, includes `<Math/Foo.h>` (ADR 0014) — inspeção
- [ ] Tipos no namespace raiz `VibeEngine`, operações em `VibeEngine::Math` (ADR 0017) — inspeção
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T04). **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `Vec2`/`Vec3`/`Vec4`/`Mat4`/`Quat`, `Transform` | fases 2+ (Renderer, Animation, Character, Camera) |
| `Math::Identity`/`Normalize`/`Length`/`Lerp`, `Math::Epsilon` | fases 2+ |

## Hardening aplicável (referência + concretização)
- §3 → aqui: `Vec3`/`Mat4`/`Transform` PascalCase (sem prefixo V — não são escalares); campos `m_`.
- §4 → aqui: headers em `Public/Math/`; include `<Math/Foo.h>` (ADR 0014); zero Private.
- §5 → aqui: Doxygen em cada typedef, free function e na struct `Transform`.
- §7 → aqui: a tabela do plano É a aplicação; FP via `WithinAbs`+epsilon nomeado.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Dependências (T02 = Implementado) → 4. Pre-flight
(`Vfloat` real vs Tasks/02 §Contratos; glm disponível) → 5. (git já bootstrapped) → 6. Status
Em-execução → 7. Baseline (suite atual verde) → 8. Branch `task/04-math-wrappers-glm` → 9. `tdd` sobre
a lista RED → 10. Gate completo → 11. Status Implementado → 12. Commit `[task 04] math: wrappers glm`,
push, PR.

## Desvios aprovados
(vazio)

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §8.3 · Phases/Fase-01-foundation.md · Decisions/0014, 0017
