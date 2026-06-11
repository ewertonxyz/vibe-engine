---
id: 0009
titulo: Testabilidade de VASSERT e hash determinístico de VStringId
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation, Tasks/02-core-types-handles-result, Decisions/0001]
---

## Contexto

A Task 02 implementa `VASSERT` e `VStringId`. Dois detalhes técnicos não cobertos pela ADR 0001 são pré-requisito para os testes Catch2 da Task 02 serem implementáveis:

1. **VASSERT em build de teste** não pode chamar `__debugbreak()` nem `std::abort()` — quebra a suite. Precisa de mecanismo de captura.
2. **`VStringId` hash** precisa ser determinístico entre execuções, máquinas e platforms — caso contrário, testes de não-colisão e qualquer tentativa futura de serialização quebram.

## Opções consideradas

- **VASSERT testável**: (A) handler instalável apenas em build de teste; (B) lança exceção em build de teste; (C) não testar.
- **VStringId hash**: (A) FNV-1a 64-bit sem seed; (B) FNV-1a com seed configurável por processo; (C) xxHash64 sem seed.

## Decisão

### VASSERT — handler instalável em build de teste

`VASSERT(Expr, ...)` expande para chamada à função `VibeEngine::Core::Detail::ReportAssertFailure(Expr, File, Line, Message)` declarada em `Core/Public/Assert.h` e definida em `Core/Private/AssertImpl.cpp`.

Em build com `VIBE_TESTING=1`:
- `AssertImpl.cpp` mantém um ponteiro de função `g_TestHandler` (default `nullptr`).
- Se `g_TestHandler != nullptr`, ele é chamado e o controle retorna normalmente (sem break/abort).
- Helper público apenas para testes: `void VASSERT_SetHandlerForTesting(void (*Handler)(const char* Expr, const char* File, int Line, const char* Message))` em `Assert.h` guardado por `#if VIBE_TESTING`.

Em build sem `VIBE_TESTING`:
- `ReportAssertFailure` loga via `VLOG_ERROR` (forward decl — Task 03 implementa Logging) e em seguida chama `__debugbreak()` no MSVC; `std::abort()` como fallback.

Em build Shipping (`VIBE_ASSERTS=0`, ADR 0008):
- `VASSERT(Expr, ...)` expande para `((void)0)` no preprocessador. `VVERIFY` mantém avaliação de `Expr` mas suprime check. `VCHECK` permanece em todos os builds (invariantes críticos).

Padrão de teste (Catch2):
```cpp
TEST_CASE("Core_Assert_FailureCapturedInTestBuild", "[core][unit]") {
    bool fired = false;
    VASSERT_SetHandlerForTesting([](auto, auto, auto, auto){ /* captura */ });
    VASSERT(false, "test");
    REQUIRE(fired);
    VASSERT_SetHandlerForTesting(nullptr);
}
```

### VStringId — FNV-1a 64-bit sem seed

`VStringId::VStringId(Vspan<const char> Text)` calcula FNV-1a 64-bit com constantes padrão:
- offset basis: `0xCBF29CE484222325`
- prime:        `0x100000001B3`

Determinístico por construção:
- Mesma string sempre vira mesmo `Vuint64`.
- Igualdade entre execuções, máquinas e platforms.
- Permite serialização futura de `VStringId` em scenes/saves sem invariantes adicionais.

Implementação `constexpr` para literais via `consteval` operator suffix:

```cpp
constexpr VStringId operator""_sid(const char* Str, std::size_t Len) noexcept;
```

Reverse-lookup table (`Vuint64 → const char*` original) apenas em builds com `VIBE_TESTING || VIBE_BUILD_DEBUG || VIBE_BUILD_DEVELOPMENT` — `DebugString()` retorna `"<unknown>"` em Shipping.

Anti-DoS via seed aleatória (motivação típica de `std::hash` ser não-determinístico) é irrelevante para engine offline first — HARDENING §2 exclui networking.

## Consequências

- `Assert.h` ganha API extra `VASSERT_SetHandlerForTesting` visível somente em build com `VIBE_TESTING=1`. Production code zero impacto.
- Testes da Task 02 podem cobrir 100% das funcionalidades de `VASSERT` (HARDENING §7).
- `VStringId` torna-se candidato natural a chaves de hash maps de gameplay/asset (estável entre runs).
- `StringIdTable` (reverse lookup) é responsabilidade do `Core` em Debug/Development; tabela some em Shipping (zero overhead).
- Compatibilidade futura: se um dia precisarmos randomizar hash (improvável), a troca é local ao construtor — consumidores não se importam.
