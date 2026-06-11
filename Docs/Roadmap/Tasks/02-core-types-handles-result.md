---
id: 02
nome: core-types-handles-result
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [01]
decisions: [0001, 0008, 0009, 0014]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: false
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0001-tipos-fundamentais-core.md
  - Docs/Decisions/0008-tooling-build-conventions.md   # tabela de defines por preset (VIBE_*)
  - Docs/Decisions/0009-vassert-test-handler-e-stringid-seed.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md   # §Contratos entre tasks, §Comandos canônicos
  - Docs/Roadmap/Tasks/01-cmake-vcpkg-presets.md  # apenas §Contratos (convenções de build)
files_create:
  - Engine/Source/Runtime/CMakeLists.txt
  - Engine/Source/Runtime/Core/CMakeLists.txt
  - Engine/Source/Runtime/Core/Public/Core/Types.h
  - Engine/Source/Runtime/Core/Public/Core/Handle.h
  - Engine/Source/Runtime/Core/Public/Core/Result.h
  - Engine/Source/Runtime/Core/Public/Core/StringId.h
  - Engine/Source/Runtime/Core/Public/Core/Assert.h
  - Engine/Source/Runtime/Core/Private/StringIdTable.cpp
  - Engine/Source/Runtime/Core/Private/AssertImpl.cpp
  - Engine/Source/Runtime/Core/Tests/Core_Types.cpp
  - Engine/Source/Runtime/Core/Tests/Core_Handle.cpp
  - Engine/Source/Runtime/Core/Tests/Core_Result.cpp
  - Engine/Source/Runtime/Core/Tests/Core_StringId.cpp
  - Engine/Source/Runtime/Core/Tests/Core_Assert.cpp
  - Engine/Source/Runtime/Core/Tests/Core_Smoke.cpp
  - Engine/Source/Tests/CMakeLists.txt
files_modify:
  - CMakeLists.txt
---

# 02 — Core: Types, Handle, Result, StringId, Assert

## Objetivo
Entregar os tipos fundamentais do módulo `Core` consumidos por toda a engine (design-mvp.md §8.1, ADR 0001/0009/0014) e fazer nascer o alvo `VibeTests` (Catch2). O executor NÃO precisa ler o design — os contratos abaixo são completos.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:` (CMakeLists.txt raiz), (4) o que ele criar. **Nada além disso** (HARDENING §14).

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4 (include via ADR 0014), §5, §7, §12, §14 |
| Decisions/0001 | semântica de VResult/Vstring/VHandle | inteiro |
| Decisions/0008 | **tabela de defines por preset** (`VIBE_ASSERTS`, `VIBE_TESTING`, `VIBE_BUILD_*` — globais, vêm dos presets da T01) | tabela "Flags e defines por preset" |
| Decisions/0009 | VASSERT testável + FNV-1a do VStringId | inteiro |
| Phases/Fase-01-foundation.md | contratos cross-task + comandos | §Contratos entre tasks, §Comandos canônicos |
| Tasks/01 | convenções de build (alvos, bin/, defines globais) | §Contratos |

## Escopo

### Dentro
- 5 headers Public (em `Public/Core/` — ADR 0014), 2 TUs Private, alvo `Vibe::Core` (STATIC), alvo `VibeTests`, 7 testes.

### Fora (NÃO fazer)
- `Logging.h`/`Time.h`/`Profile.h` (T03). `MemoryTag` (T05). Math (T04).
- Nenhum include de spdlog ou `<windows.h>` em lugar nenhum desta task.
- Nenhum `target_compile_definitions` para `VIBE_*` — esses defines são **globais por preset** (T01/ADR 0008); adicioná-los por alvo é violação.

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create`/`files_modify` exaustivos. Exceções permanentes: este doc, README do roadmap, Backlog. Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) um contrato não compila (ex.: `std::expected` indisponível); (b) precisar de arquivo fora das listas; (c) seções deste doc se contradizem; (d) teste do plano é impossível; (e) gate falha após 2 ciclos de `diagnose`. Relato: **passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <Core/Foo.h>` — ADR 0014)

### Engine/Source/Runtime/Core/Public/Core/Types.h
```cpp
namespace VibeEngine
{
using Vint8  = std::int8_t;   using Vuint8  = std::uint8_t;
using Vint16 = std::int16_t;  using Vuint16 = std::uint16_t;
using Vint32 = std::int32_t;  using Vuint32 = std::uint32_t;
using Vint64 = std::int64_t;  using Vuint64 = std::uint64_t;
using Vfloat = float;         using Vdouble = double;
using Vbyte  = std::byte;
template <typename T> using Vspan = std::span<T>;
using Vstring = std::string;  // ADR 0001 — alias simples no MVP
}
```

### Engine/Source/Runtime/Core/Public/Core/Result.h
```cpp
namespace VibeEngine
{
template <typename T, typename E> using VResult = std::expected<T, E>;  // ADR 0001
}
```

### Engine/Source/Runtime/Core/Public/Core/Handle.h
```cpp
namespace VibeEngine
{
template <typename T>
class alignas(8) VHandle
{
public:
    constexpr VHandle() noexcept = default;
    constexpr VHandle(Vuint32 Index, Vuint32 Generation) noexcept;
    static constexpr VHandle Invalid() noexcept;          // ADR 0001
    constexpr Vuint32 Index() const noexcept;
    constexpr Vuint32 Generation() const noexcept;
    constexpr bool IsValid() const noexcept;               // Generation 0 = inválido
    constexpr bool operator==(const VHandle&) const noexcept = default;
private:
    Vuint32 m_Index { 0 };
    Vuint32 m_Generation { 0 };
};
static_assert(sizeof(VHandle<struct VHandleSizeProbe>) == 8);
}
// Especialização std::hash<VibeEngine::VHandle<T>>:
//   return std::hash<Vuint64>{}((static_cast<Vuint64>(H.Generation()) << 32) | H.Index());
```

### Engine/Source/Runtime/Core/Public/Core/StringId.h
```cpp
namespace VibeEngine
{
class VStringId
{
public:
    constexpr VStringId() noexcept = default;
    explicit constexpr VStringId(Vspan<const char> Text) noexcept;  // ADR 0009/0001
    constexpr Vuint64 Value() const noexcept;
    const char* DebugString() const noexcept;  // tabela reversa; "<unknown>" em Shipping
    constexpr bool operator==(const VStringId&) const noexcept = default;
private:
    Vuint64 m_Value { 0 };
};
constexpr VStringId operator""_sid(const char* Str, std::size_t Len) noexcept; // constrói o Vspan e delega
}
```

### Engine/Source/Runtime/Core/Public/Core/Assert.h
```cpp
namespace VibeEngine::Core::Detail
{
void ReportAssertFailure(const char* Expr, const char* File, int Line, const char* Message);
void SetAssertLogHook(void (*Hook)(const char* Formatted));  // T03 conecta ao VLOG_ERROR
}
// VASSERT(Expr) ou VASSERT(Expr, "mensagem literal"): sem mensagem → Message = "".
// SEM formatação printf no MVP (mensagem é um literal const char* — decisão desta criação).
#define VASSERT(Expr, ...)  /* ativo se VIBE_ASSERTS=1; ((void)0) em Shipping */
#define VVERIFY(Expr, ...)  /* SEMPRE avalia Expr (exatamente 1×); check só se VIBE_ASSERTS=1 */
#define VCHECK(Expr, ...)   /* ativo em TODOS os builds */
// Falha de qualquer um dos três chama Detail::ReportAssertFailure.
#if VIBE_TESTING
namespace VibeEngine
{
void VASSERT_SetHandlerForTesting(void (*Handler)(const char* Expr, const char* File, int Line, const char* Message));
}
#endif
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **Defines `VIBE_*` são globais por preset** (T01, tabela da ADR 0008 — no contexto): `debug` e `development` têm `VIBE_TESTING=1` e `VIBE_ASSERTS=1`; `shipping` tem ambos `=0`. Nenhum define extra nesta task.
- FNV-1a 64-bit (ADR 0009), sem seed: `Hash = 0xCBF29CE484222325; para cada byte B do texto: Hash ^= B; Hash *= 0x100000001B3;`.
- Construtor do `VStringId` é `constexpr`; o registro na tabela reversa usa **`if !consteval`** (C++23): só o caminho runtime registra em `StringIdTable.cpp` (tabela = `std::unordered_map<Vuint64, Vstring>`). Literais `_sid` avaliados em compile-time não entram na tabela — `DebugString()` deles retorna `"<unknown>"` (comportamento esperado pelo teste 6).
- `ReportAssertFailure` (AssertImpl.cpp): com `VIBE_TESTING=1` e handler instalado → chama handler e RETORNA; sem handler → hook de log (default `fprintf(stderr, ...)`) + `__debugbreak()` (ADR 0009).
- O handler de teste é **ponteiro de função puro** — capturar estado via flags `static` no arquivo de teste, NUNCA lambda capturante (o snippet da ADR 0009 é ilustrativo e não compila como está).
- Includes: cada header Public inclui apenas o que usa (ex.: `<cstdint>`, `<cstddef>`, `<span>`, `<string>`, `<expected>`); nada de Private/, spdlog ou `<windows.h>` (HARDENING §4).
- Doxygen `/** @brief */` em TODA declaração pública (HARDENING §5) — o executor escreve as tags completas.
- CMake (ADR 0014): `target_include_directories(VibeCore PUBLIC <dir>/Public PRIVATE <dir>/Private)`; testes e código usam `#include <Core/Types.h>` etc.; `VibeTests` linka `Vibe::Core` + `Catch2::Catch2WithMain` sobre `Core/Tests/*.cpp`; `catch_discover_tests(VibeTests)`. Raiz (files_modify) ganha, no marcador da T01: `add_subdirectory(Engine/Source/Runtime)` e `add_subdirectory(Engine/Source/Tests)`; o novo `Engine/Source/Runtime/CMakeLists.txt` chama `add_subdirectory(Core)`.

## Plano de testes (lista RED — ordem de execução do `tdd`)

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Core_Smoke_Task02` | `[core][smoke]` | Core/Tests/Core_Smoke.cpp | handle válido; reuse de index com generation nova ≠ antigo; `VResult` ok/err; `"Player"_sid == "Player"_sid`; VASSERT capturado por handler; < 1 s |
| 2 | `Core_Types_FixedSizes` | `[core][unit]` | Core/Tests/Core_Types.cpp | `sizeof` de todos os aliases (`Vint8`..`Vuint64`, `Vfloat`, `Vdouble`, `Vbyte==1`); `Vspan<const char>` sobre literal tem `size()` correto; `Vstring` round-trip básico |
| 3 | `Core_Handle_GenerationDistinguishesReuse` | `[core][unit]` | Core/Tests/Core_Handle.cpp | `VHandle<T>(5,1) != VHandle<T>(5,2)`; `std::hash` distinto para os dois |
| 4 | `Core_Handle_InvalidIsNotValid` | `[core][unit]` | Core/Tests/Core_Handle.cpp | default e `Invalid()` não-válidos; `VHandle<T>(0,1).IsValid()==true`; `VHandle<T>(5,7).Index()==5 && .Generation()==7` |
| 5 | `Core_Result_OkErrPropagation` | `[core][unit]` | Core/Tests/Core_Result.cpp | valor → `has_value()`; erro → `.error()` correto; `and_then` propaga |
| 6 | `Core_StringId_StableHashAndRoundtrip` | `[core][unit]` | Core/Tests/Core_StringId.cpp | mesmo texto → mesmo `Value()` (runtime e `_sid`); `Value()!=0` p/ não-vazio; `DebugString()` devolve o texto p/ id runtime; `_sid` compile-time → `"<unknown>"` |
| 7 | `Core_Assert_FailureCapturedInTestBuild` | `[core][unit]` | Core/Tests/Core_Assert.cpp | handler captura Expr/File/Line do `VASSERT`; `VVERIFY(f())` avalia `f` exatamente 1× e dispara handler em falso; `VCHECK(false)` dispara handler; teardown restaura `nullptr` |

**Smoke**: teste #1, orçamento < 30 s (HARDENING §7). **Determinismo**: zero RNG, zero wall-clock. **Cobertura**: cada símbolo público dos Contratos aparece em ≥ 1 linha acima — exceção registrada: `Detail::SetAssertLogHook` é coberto na T03 (`Core_Logging_AssertHookConnected`), que é quem o conecta.

## Comandos (fonte: Fase-01 §Comandos canônicos + variantes por task com filtro `-R "Core_"` e tags `[core][smoke]`)

```powershell
cmake --preset debug
cmake --preset development
cmake --build --preset debug
cmake --build --preset development
ctest --preset debug --output-on-failure -R "Core_"
Build\debug\bin\VibeTests.exe "[core][smoke]" --durations yes
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
```

## Critério de aceitação
- [ ] 7 TEST_CASE do plano verdes — `ctest --preset debug -R "Core_"`
- [ ] Smoke `Core_Smoke_Task02` < 1 s — saída de `--durations yes`
- [ ] Suite completa verde em debug e development
- [ ] Headers Public em `Public/Core/` e includes na forma `<Core/Foo.h>` (ADR 0014) — inspeção
- [ ] Headers Public sem include de Private/, `<windows.h>`, spdlog — inspeção dos 5 headers
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK (V-prefix, m_PascalCase, arquivo = tipo)
- [ ] vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T02). **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| aliases de `Types.h` (grupo) | T03–T14 |
| `VHandle<T>` (`Invalid/Index/Generation/IsValid/==`) | T05, T09, T11 e fases 2+ |
| `VResult<T,E>` | T07, T11, T12, T13 |
| `VStringId` / `operator""_sid` | fases 5+ |
| `VASSERT`/`VVERIFY`/`VCHECK` + `Detail::SetAssertLogHook` | T03 (conecta o hook), todas |

## Hardening aplicável (referência + concretização)
- §3 → aqui: `m_Index`/`m_Generation`/`m_Value`; V-prefix em todos os tipos.
- §4 → aqui: headers em `Public/Core/`; include `<Core/Foo.h>` (ADR 0014); zero acoplamento com Logging.
- §5 → aqui: Doxygen completo nos 5 headers.
- §7 → aqui: a tabela do plano É a aplicação; teste 7 restaura o handler no teardown.
- §12 → aqui: tipos POD/alias, zero alocação própria (tabela reversa usa contêiner padrão em build de dev — aceito, ADR 0009).

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Dependências (T01 = Implementado) →
4. Pre-flight: convenções da T01 existem (presets, bin/, defines globais) → 5. Bootstrap git
(já feito na T01) → 6. Status Em-execução → 7. Baseline (build + suite, vazia é válida) →
8. Branch `task/02-core-types-handles-result` → 9. `tdd` sobre a lista RED → 10. Gate completo →
11. Status Implementado → 12. Commit `[task 02] core: types, handle, result, stringid, assert`, push, PR.

## Desvios aprovados
(vazio)

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §8.1 · Phases/Fase-01-foundation.md · Decisions/0001, 0008, 0009, 0014
