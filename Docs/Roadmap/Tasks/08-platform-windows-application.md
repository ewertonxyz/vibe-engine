---
id: 08
nome: platform-windows-application
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [07]
decisions: [0014]
especialistas_consultados: [vx-spec-architecture, vx-spec-testing]
risco_memoria: false
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md   # §Contratos entre tasks (T08), §Comandos canônicos
  - Docs/Roadmap/Tasks/07-platform-windows-base.md  # apenas §Contratos (PlatformError, módulo Platform/CMake)
  - Docs/Roadmap/Tasks/02-core-types-handles-result.md  # apenas §Contratos (VResult, Vuint8)
files_create:
  - Engine/Source/Runtime/Platform/Public/Platform/PlatformApplication.h
  - Engine/Source/Runtime/Platform/Private/Windows/WindowsApplication.cpp
  - Engine/Source/Runtime/Platform/Tests/Platform_Application.cpp
files_modify:
  - Engine/Source/Runtime/Platform/CMakeLists.txt
---

# 08 — Platform: Application (lifecycle + message pump headless)

## Objetivo
Última primitiva da camada Platform da Fase 1 (design-mvp.md §8.2 "WindowsApplication — main loop, message
pump"): `PlatformApplication`, o lifecycle de processo + bomba de mensagens do SO, **sem janela** (Window é
Fase 2). Headless e testável: `Initialize` → `PumpMessages` (não-bloqueante) → `RequestQuit` → `Shutdown`.
Toda interação Win32 fica em PImpl no `.cpp`. O executor NÃO precisa ler o design.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso**
(HARDENING §14). T07 ainda não está implementada na criação → o pre-flight compara contra `Tasks/07 §Contratos`.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4, §5, §7, §14 |
| Decisions/0014 | include `<Platform/Foo.h>` / `<Core/Foo.h>` | inteiro |
| Phases/Fase-01 | contrato T08 + comandos | §Contratos entre tasks (T08), §Comandos canônicos |
| Tasks/07 | `PlatformError`; módulo `Platform` (alvo `VibePlatform`, `Private/Windows/`, CMake já criado) | §Contratos |
| Tasks/02 | `VResult`, `Vuint8` | §Contratos |

## Escopo

### Dentro
- 1 header Public (`PlatformApplication`), 1 TU Windows, 5 testes.
- Lifecycle idempotente; `PumpMessages` não-bloqueante drenando 1 passada da fila de mensagens da thread.

### Fora (NÃO fazer, mesmo que pareça óbvio)
- `Window`, `RegisterClass`/`CreateWindow`, WndProc, swapchain, loop de render/jogo (Fase 2/12).
- `GetMessage` (bloqueia) — proibido; usar `PeekMessage` (não-bloqueante).
- Tocar `Engine/Source/Runtime/CMakeLists.txt` ou `Engine/Source/Tests/CMakeLists.txt` (T07 já os ligou; `Vibe::Platform` já linkado, fontes auto-globadas — ADR 0016).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_modify` é SÓ `Platform/CMakeLists.txt` (adicionar 1 source a `VibePlatform`). Exceções permanentes:
este doc, README, Backlog. Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE quando: (a) contrato não compila vs `<Core/...>`/`<Platform/PlatformError.h>` reais; (b) precisar de
arquivo fora das listas (ex.: `Vibe::Platform` NÃO linkado em VibeTests → drift da T07); (c) seções se
contradizem; (d) teste impossível; (e) gate falha após 2 ciclos de `diagnose`. Relate: **passo · trecho do
doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; `#include <Platform/PlatformApplication.h>`. Doxygen §5 completo.)

### Public/Platform/PlatformApplication.h
```cpp
#include <Core/Result.h>
#include <Core/Types.h>
#include <Platform/PlatformError.h>

#include <memory>   // std::unique_ptr (PImpl)

namespace VibeEngine {
/// @brief Resultado de uma passada de `PumpMessages`.
enum class PumpResult : Vuint8 {
    Continue,       ///< @brief Sem quit pendente; o loop deve seguir.
    QuitRequested   ///< @brief WM_QUIT drenado; o loop deve sair.
};

/**
 * @brief Lifecycle de processo + bomba de mensagens do SO, sem janela (Fase 1).
 * @details Win32 (PeekMessage/PostQuitMessage) confinado ao `.cpp` via PImpl — sem `<windows.h>` no header.
 *          Instância única de app: não-copiável e não-movível.
 */
class PlatformApplication {
public:
    PlatformApplication();                               ///< @brief Constrói o app (ainda não inicializado).
    ~PlatformApplication();                              ///< @brief Destrói; chama Shutdown se necessário (definido no .cpp — Impl incompleto).
    PlatformApplication(const PlatformApplication&) = delete;
    PlatformApplication& operator=(const PlatformApplication&) = delete;
    PlatformApplication(PlatformApplication&&) = delete;
    PlatformApplication& operator=(PlatformApplication&&) = delete;

    /// @brief Adquire o estado de processo / prepara a fila de mensagens. Idempotente. @return ok ou PlatformError.
    VResult<void, PlatformError> Initialize();
    /// @brief Drena as mensagens pendentes uma vez, sem bloquear. @return QuitRequested se houve quit; senão Continue.
    PumpResult PumpMessages();
    /// @brief Solicita encerramento (posta um quit na fila da thread). Idempotente.
    void RequestQuit();
    /// @brief Libera o estado adquirido em Initialize. Idempotente.
    void Shutdown();
    /// @brief @return true entre um Initialize bem-sucedido e o Shutdown correspondente.
    bool IsInitialized() const;

private:
    struct Impl;                ///< @brief Estado Win32 opaco; definido só no `.cpp`.
    std::unique_ptr<Impl> m_Impl;
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **PImpl com `std::unique_ptr<Impl>`** mantém `<windows.h>` fora do header (§4, ADR 0014). `Impl` é forward-declarado no header e definido no `.cpp`; por isso `~PlatformApplication()` é **declarado no header e definido no `.cpp`** (NÃO `= default` inline — `Impl` incompleto). As 4 special members `= delete` (instância única).
- **Sem janela**: `PumpMessages` opera na fila de mensagens da thread (mensagens `HWND==nullptr`):
  `while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE)) { if (Msg.message==WM_QUIT) { marca quit; break; } TranslateMessage; DispatchMessage; }`. `RequestQuit` → `PostQuitMessage(0)`. **Proibido `GetMessage`/`RegisterClass`/`CreateWindow`** (Fase 2).
- **Idempotência**: `Initialize` já-inicializado = no-op com sucesso; `Shutdown` sem Init / repetido = no-op; `PumpMessages` fora do par Init/Shutdown = `Continue`. Re-inicializar após Shutdown é permitido (novo ciclo). `IsInitialized()` reflete o estado.
- `WindowsApplication.cpp` (nome de backend; o header público é neutro). `<windows.h>` com `WIN32_LEAN_AND_MEAN`/`NOMINMAX`. Único heap: o `unique_ptr<Impl>` (código frio, permitido §12). Sem C-cast.
- **CMake**: `Platform/CMakeLists.txt` (files_modify) ganha `Private/Windows/WindowsApplication.cpp` na lista de fontes de `VibePlatform`. NADA mais.

## Plano de testes (lista RED — ordem de execução do `tdd`)
Determinístico e headless: tudo na thread do teste (a fila `WM_QUIT` é thread-local); sem janela, sem sleep, sem wall-clock, sem loop bloqueante.

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Platform_Smoke_Task08` | `[platform][smoke]` | Platform_Application.cpp | `Initialize()` ok; `IsInitialized()`; `PumpMessages()` 3× == `Continue`; `RequestQuit()`; próximo `PumpMessages()` == `QuitRequested`; `Shutdown()`; `!IsInitialized()`; < 1 s |
| 2 | `Platform_App_RequestQuitPumpReportsQuit` | `[platform][unit]` | Platform_Application.cpp | após `Initialize()`, `PumpMessages()` == `Continue`; após `RequestQuit()`, o próximo `PumpMessages()` == `QuitRequested` |
| 3 | `Platform_App_PumpNonBlockingBeforeQuit` | `[platform][unit]` | Platform_Application.cpp | após `Initialize()`, loop fixo (`constexpr int N=5`) de `PumpMessages()` cada == `Continue` e completa (não bloqueia) |
| 4 | `Platform_App_InitShutdownIdempotent` | `[platform][unit]` | Platform_Application.cpp | `Initialize()` 2× ok; `Shutdown()` 2× seguro; novo ciclo `Initialize()`→`Shutdown()` funciona; `IsInitialized()` coerente |
| 5 | `Platform_App_NonCopyableNonMovable` | `[platform][unit]` | Platform_Application.cpp | `static_assert(!is_copy_constructible_v && !is_move_constructible_v<PlatformApplication>)` |

**Smoke**: teste #1, < 30 s. **Determinismo** (HARDENING §7): sem wall-clock/RNG/sleep; bombas na thread do
teste; sem janela real; iterações são `constexpr`. **Cobertura**: `Initialize`/`PumpMessages`/`RequestQuit`/
`Shutdown`/`IsInitialized` + `PumpResult` aparecem em ≥ 1 linha. **risco_memoria/frame false** → sem gate
asan extra além do build; sem teste `[perf]`.

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos)

```powershell
cmake --preset debug
cmake --preset development
cmake --build --preset debug
cmake --build --preset development
ctest --preset debug --output-on-failure -R "Platform_App|Platform_Smoke_Task08"
Build\debug\bin\VibeTests.exe "[platform][smoke]" --durations yes
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
```

## Critério de aceitação
- [ ] 5 TEST_CASE do plano verdes — `ctest --preset debug -R "Platform_App|Platform_Smoke_Task08"`
- [ ] Smoke `Platform_Smoke_Task08` < 1 s — `--durations yes`
- [ ] Suite completa verde em debug e development (não regrediu; Platform da T07 intacto)
- [ ] `PlatformApplication.h` sem `<windows.h>` (PImpl); `<windows.h>` só em `WindowsApplication.cpp` — inspeção
- [ ] `PumpMessages` não-bloqueante (PeekMessage, não GetMessage) — inspeção
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T08). **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `PlatformApplication` (`Initialize`/`PumpMessages`/`RequestQuit`/`Shutdown`/`IsInitialized`), `PumpResult` | T13 (EngineCore), T14 (smoke) |

## Hardening aplicável (referência + concretização)
- §3 → aqui: `PlatformApplication`/`PumpResult` PascalCase; `m_Impl`.
- §4 → aqui: Public sem `<windows.h>` (PImpl); `<windows.h>` só no `.cpp`; include `<Platform/Foo.h>` (ADR 0014).
- §5 → aqui: Doxygen completo no header.
- §7 → aqui: a tabela do plano É a aplicação; headless, determinístico, sem bloqueio.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Dependências (T07 = Implementado) → 4. Pre-flight
(`PlatformError` real vs Tasks/07; `VResult` vs Tasks/02) → 5. (git já bootstrapped) → 6. Status Em-execução →
7. Baseline (suite Platform da T07 verde) → 8. Branch `task/08-platform-windows-application` → 9. `tdd` sobre a
lista RED → 10. Gate completo → 11. Status Implementado → 12. Commit `[task 08] platform: application lifecycle`, push, PR.

## Desvios aprovados
(vazio)

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §8.2 · Phases/Fase-01-foundation.md · Decisions/0014
