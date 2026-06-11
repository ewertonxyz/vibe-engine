---
id: 0001
titulo: Tipos fundamentais do módulo Core
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation]
---

## Contexto

A Fase 1 (Foundation) cria os tipos fundamentais que todo o restante da engine consumirá. O design-mvp.md §8.1 lista `Vint*`, `Vuint*`, `Vfloat`, `Vdouble`, `Vbyte`, `Vspan`, `Vstring`, `VStringId`, `VHandle<T>`, `VResult<T,E>`, mas não fixa a implementação interna de quatro tipos cuja escolha tem impacto cross-module:

- `VResult<T,E>` — pode ser implementação própria ou alias para `std::expected` (C++23).
- `Vstring` — pode ser `std::string`, `std::pmr::string` (allocator-aware) ou tipo próprio com SSO controlável.
- `VHandle<T>` — o design diz "handle tipado com generation counter", mas não obriga a entrega já na Fase 1.
- `MemoryTag` — categoria por subsistema (§8.4), pode ser `enum class` fechado ou `VStringId` aberto.

Erros nesta camada são caros: cada tipo é consumido por dezenas de cabeçalhos em fases posteriores; troca pós-MVP exige churn massivo.

## Opções consideradas

- **VResult**: (A) `std::expected` com alias `using VResult = std::expected`; (B) implementação própria.
- **Vstring**: (A) `std::string`; (B) `std::pmr::string`; (C) tipo próprio.
- **VHandle**: (A) `{index, generation}` 8B já na Fase 1; (B) `{index}` 4B agora, generation depois.
- **MemoryTag**: (A) `enum class` fechado; (B) `VStringId` aberto.

## Decisão

| Tipo | Escolha | Razão |
|---|---|---|
| `VResult<T,E>` | `std::expected` com `template<typename T, typename E> using VResult = std::expected<T,E>;` | C++23 está na stack (§4); zero código a manter; interop com STL. |
| `Vstring` | `using Vstring = std::string;` | Simplicidade no MVP; SSO do MSVC já cobre identificadores curtos. Allocator-aware adiado para pós-MVP se algum hotspot exigir. |
| `VHandle<T>` | `struct VHandle<T> { Vuint32 m_Index; Vuint32 m_Generation; };` (8 B) | Cumprimento literal do §8.1; detecta use-after-free desde o início; cabe em registrador. |
| `MemoryTag` | `enum class MemoryTag : Vuint16 { Core, Job, FileSystem, Debug, Frame, ... };` | Categoria fechada conhecida; switch direto, cache-friendly. Plugins (que poderiam exigir aberto) estão na lista negativa HARDENING §2. |

## Consequências

- **VResult**: API é a do `std::expected` — `.value()`, `.error()`, `.transform()`, `.and_then()`. Hooks de telemetria/logging em erros, se necessários, ficam em camada externa (wrapper de função, não no tipo).
- **Vstring**: APIs que precisam de allocator-aware string usarão `std::pmr::string` localmente ou exposed param `Vspan<const char>`. Toda função pública que recebe string aceita `Vspan<const char>` (não `const Vstring&`), evitando acoplamento com a representação interna.
- **VHandle**: cabeçalho `Handle.h` em `Core/Public/` expõe construtor explícito, `operator==`, hash, `IsValid()` e helper estático `Invalid()`. Generation counter incrementa no destrutor do recurso dono.
- **MemoryTag**: novas tags exigem PR no Core; cada subsistema novo adiciona sua entrada em PR conjunto. Aceita-se esse atrito.

Referências para tasks: qualquer task da Fase 1 que tocar `Core/Public/` cita esta ADR.
