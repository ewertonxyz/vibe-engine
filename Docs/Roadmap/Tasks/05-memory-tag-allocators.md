---
id: 05
nome: memory-tag-allocators
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [02]
decisions: [0001, 0003, 0014]
especialistas_consultados: [vx-spec-memory-perf, vx-spec-architecture, vx-spec-testing]
risco_memoria: true
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0001-tipos-fundamentais-core.md
  - Docs/Decisions/0003-allocators-e-diagnostico.md
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md   # §Contratos entre tasks (T05/T06), §Comandos canônicos
  - Docs/Roadmap/Tasks/01-cmake-vcpkg-presets.md  # apenas §Contratos (build, asan-debug, bin/)
  - Docs/Roadmap/Tasks/02-core-types-handles-result.md  # apenas §Contratos (Vuint*/Vbyte/Vspan, VASSERT)
files_create:
  - Engine/Source/Runtime/Memory/CMakeLists.txt
  - Engine/Source/Runtime/Memory/Public/Memory/MemoryTag.h
  - Engine/Source/Runtime/Memory/Public/Memory/IAllocator.h
  - Engine/Source/Runtime/Memory/Public/Memory/LinearAllocator.h
  - Engine/Source/Runtime/Memory/Private/MemoryTag.cpp
  - Engine/Source/Runtime/Memory/Private/LinearAllocator.cpp
  - Engine/Source/Runtime/Memory/Tests/Memory_Linear.cpp
  - Engine/Source/Runtime/Memory/Tests/Memory_Tag.cpp
  - Engine/Source/Runtime/Memory/Tests/Memory_Smoke.cpp
files_modify:
  - Engine/Source/Runtime/CMakeLists.txt
  - Engine/Source/Tests/CMakeLists.txt
---

# 05 — Memory: MemoryTag, IAllocator, LinearAllocator

## Objetivo
Primeira fatia do módulo `Memory` (design-mvp.md §8.4, ADR 0001/0003): `MemoryTag` (enum fechado
`Vuint16`), a interface `IAllocator` (slot multi-backend), e `LinearAllocator` (bump allocator que
POSSUI seu buffer, com agregação de bytes por-tag). FrameAllocator e TrackingAllocator ficam para a
T06. O executor NÃO precisa ler o design — os contratos abaixo são completos.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além
disso** (HARDENING §14).

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4, §5, §7, §12 (memória), §14 |
| Decisions/0001 | MemoryTag = enum class Vuint16 fechado | inteiro |
| Decisions/0003 | LinearAllocator concreto; Free whole-arena; new global NÃO sobrescrito; asan opt-in | inteiro |
| Decisions/0014 | include `<Memory/Foo.h>` / `<Core/Types.h>` a partir de Public/ | inteiro |
| Phases/Fase-01 | contrato T05/T06 + comandos (inclui asan) | §Contratos entre tasks, §Comandos canônicos |
| Tasks/01 | preset asan-debug; alvo/teste; bin/ | §Contratos |
| Tasks/02 | `Vuint64`/`Vbyte`/`Vspan` de `<Core/Types.h>`, `VASSERT` de `<Core/Assert.h>` | §Contratos |

## Escopo

### Dentro
- 3 headers Public em `Public/Memory/` (ADR 0014), 2 TUs Private, alvo `Vibe::Memory` (STATIC), 6 testes.
- `LinearAllocator` POSSUI um bloco de heap dimensionado na construção (RAII).
- Agregação de bytes por-tag POR INSTÂNCIA (`LinearAllocator::BytesAllocated(MemoryTag)`).

### Fora (NÃO fazer)
- `FrameAllocator`, `TrackingAllocator`, `StackAllocator`, `PoolAllocator` (T06+).
- Override global de `operator new`/`delete` (ADR 0003 — proibido).
- Registry global de tags / relatório de leak cross-allocator (T06).
- Nenhum `target_compile_definitions` para `VIBE_*` (globais por preset — T01/ADR 0008).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create`/`files_modify` exaustivos. Exceções permanentes: este doc, README, Backlog. Outro
arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) contrato não compila vs `<Core/Types.h>`/`<Core/Assert.h>`
reais; (b) precisar de arquivo fora das listas; (c) seções se contradizem; (d) teste impossível;
(e) gate (incl. asan) falha após 2 ciclos de `diagnose`. Relate: **passo · trecho do doc · erro
literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <Memory/Foo.h>`)

### Engine/Source/Runtime/Memory/Public/Memory/MemoryTag.h
```cpp
#include <Core/Types.h>   // Vuint16

namespace VibeEngine
{
/**
 * @brief Categoria de subsistema usada para agregar bytes alocados (conjunto FECHADO — ADR 0001).
 */
enum class MemoryTag : Vuint16
{
    Core,        ///< @brief Infra geral / Core.
    Job,         ///< @brief JobSystem (T09/T10).
    FileSystem,  ///< @brief I/O e buffers de arquivo.
    Frame,       ///< @brief Escopo de frame (FrameAllocator, T06).
    Debug,       ///< @brief Diagnóstico/profiling.
    Count        ///< @brief Sentinela: número de tags. NÃO é categoria válida.
};

/// @brief Nome estável e legível de uma tag. @param Tag categoria @return literal ("Core"...); "Unknown" se inválida.
const char* ToString(MemoryTag Tag) noexcept;
}
```

### Engine/Source/Runtime/Memory/Public/Memory/IAllocator.h
```cpp
#include <Core/Types.h>        // Vuint64, Vbyte, Vspan
#include <Memory/MemoryTag.h>

namespace VibeEngine
{
/**
 * @brief Interface mínima de alocação (slot multi-backend; ADR 0003 — opt-in, sem override global de new).
 * @details Retorna/recebe `Vspan<Vbyte>` (nunca ptr+size cru — HARDENING §12).
 */
class IAllocator
{
public:
    /// @brief Destrutor virtual (posse polimórfica permitida).
    virtual ~IAllocator() = default;

    /**
     * @brief Aloca um bloco alinhado, categorizado por Tag.
     * @param Size Bytes (> 0; 0 → span vazio).
     * @param Alignment Potência de 2 (use DefaultAlignment se indiferente).
     * @param Tag Categoria contabilizada.
     * @return Span de `Size` bytes; span VAZIO (data()==nullptr) em OOM. `noexcept`, nunca aborta.
     */
    virtual Vspan<Vbyte> Allocate(Vuint64 Size, Vuint64 Alignment, MemoryTag Tag) noexcept = 0;

    /**
     * @brief Libera um bloco. @param Block span retornado por Allocate (vazio = no-op).
     * @details LinearAllocator: NO-OP (memória volta só via Reset()). Existe para uniformidade.
     */
    virtual void Free(Vspan<Vbyte> Block) noexcept = 0;

    /// @brief Alinhamento default sugerido (16 B, SIMD-safe x64).
    static constexpr Vuint64 DefaultAlignment = 16;

protected:
    IAllocator() = default;
};
}
```

### Engine/Source/Runtime/Memory/Public/Memory/LinearAllocator.h
```cpp
#include <Core/Types.h>
#include <Memory/IAllocator.h>
#include <Memory/MemoryTag.h>

#include <array>

namespace VibeEngine
{
/**
 * @brief Bump allocator de thread único que POSSUI seu buffer; nunca libera por bloco.
 * @details Free() é no-op; toda a arena volta via Reset(). Agrega bytes por-tag por instância.
 *          Não thread-safe (posse única — HARDENING §12).
 */
class LinearAllocator final : public IAllocator
{
public:
    /**
     * @brief Constrói e adquire um bloco de backing de `CapacityBytes` (RAII).
     * @param CapacityBytes Tamanho fixo da arena (> 0). Única alocação bruta da task (em Memory/ — §12).
     * @param OwnerTag Tag atribuída ao próprio bloco de backing.
     */
    explicit LinearAllocator(Vuint64 CapacityBytes, MemoryTag OwnerTag = MemoryTag::Core);

    /// @brief Libera o bloco de backing (RAII).
    ~LinearAllocator() override;

    LinearAllocator(const LinearAllocator&) = delete;            ///< @brief Não-copiável.
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) = delete;                 ///< @brief Não-movível (posse única em local fixo).
    LinearAllocator& operator=(LinearAllocator&&) = delete;

    Vspan<Vbyte> Allocate(Vuint64 Size, Vuint64 Alignment, MemoryTag Tag) noexcept override;
    void Free(Vspan<Vbyte> Block) noexcept override;   ///< @brief No-op (ver Reset()).

    /// @brief Rebobina o cursor ao início; zera a agregação por-tag. NÃO desaloca o backing.
    void Reset() noexcept;

    /// @brief Bytes vivos atribuídos a `Tag` nesta instância. @param Tag categoria @return bytes.
    Vuint64 BytesAllocated(MemoryTag Tag) const noexcept;

    /// @brief Bytes em uso (cursor). @return offset atual.
    Vuint64 UsedBytes() const noexcept;
    /// @brief Capacidade total do backing. @return bytes.
    Vuint64 CapacityBytes() const noexcept;

private:
    Vbyte* m_Base { nullptr };       ///< @brief Início do backing (posse).
    Vuint64 m_Capacity { 0 };        ///< @brief Tamanho do backing.
    Vuint64 m_Offset { 0 };          ///< @brief Cursor de bump (relativo a m_Base).
    MemoryTag m_OwnerTag { MemoryTag::Core }; ///< @brief Tag do backing.
    std::array<Vuint64, static_cast<Vuint64>(MemoryTag::Count)> m_TagBytes {}; ///< @brief Bytes por-tag.
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **Backing possuído**: o ctor faz UMA alocação bruta de `CapacityBytes` (`::operator new[]`/`std::malloc`) —
  único `new`/`malloc` cru permitido, confinado a `LinearAllocator.cpp` em `Memory/` (HARDENING §12); liberado
  no destrutor (RAII). Sem C-cast: `static_cast`/`reinterpret_cast` na aritmética de ponteiro.
- **Alinhamento**: `Aligned = (m_Offset + (Alignment-1)) & ~(Alignment-1)`; cabe se `Aligned + Size <= m_Capacity`.
  Em sucesso `m_Offset = Aligned + Size` e `m_TagBytes[Tag] += Size`; `VASSERT(Alignment potência de 2)`. `Size==0` → span vazio.
- **OOM** (não cabe) → span vazio (`{}`), `noexcept`, sem abort, estado inalterado, allocator segue usável.
- **`Free` é no-op** (ADR 0003 free whole-arena). **`Reset()`** zera `m_Offset` e todo o `m_TagBytes`; não desaloca o backing.
- **Agregação POR INSTÂNCIA** (sem registry global — isolamento de teste §7). `BytesAllocated(Count)` → `VASSERT`. `ToString` em `MemoryTag.cpp`.
- **Ordem do enum** `MemoryTag` segue o §Contratos da fase (autoritativo); a ordem em ADR 0001 é ilustrativa/não-vinculante.
- CMake: `Memory/CMakeLists.txt` → `add_library(VibeMemory STATIC Private/MemoryTag.cpp Private/LinearAllocator.cpp)`
  + alias `Vibe::Memory`; `target_include_directories(... PUBLIC Public PRIVATE Private)`;
  `target_link_libraries(VibeMemory PUBLIC Vibe::Core)`. `Engine/Source/Runtime/CMakeLists.txt` ganha
  `add_subdirectory(Memory)`. `Engine/Source/Tests/CMakeLists.txt` ganha `Vibe::Memory` no
  `target_link_libraries(VibeTests ...)` (fontes já globadas — ADR 0016; link é explícito).
- `Vuint64` para tamanhos (não `std::size_t`) — padrão dos contratos da fase.

## Plano de testes (lista RED — ordem de execução do `tdd`)

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Memory_Smoke_Task05` | `[memory][smoke]` | Memory/Tests/Memory_Smoke.cpp | constrói `LinearAllocator(4096)`; aloca 2 blocos tagueados via `IAllocator&`; escreve/lê nos bytes; `BytesAllocated` > 0; `Reset()` zera; destrói sem leak (asan); < 1 s |
| 2 | `Memory_Tag_EnumClosedVuint16` | `[memory][unit]` | Memory/Tests/Memory_Tag.cpp | `underlying_type_t<MemoryTag>` == `Vuint16`; `Count` estável; tags nomeadas têm valores distintos; `ToString(Core)=="Core"` etc. |
| 3 | `Memory_Tag_AggregateByCategory` | `[memory][unit]` | Memory/Tests/Memory_Tag.cpp | após Allocate(100,_,Core)+Allocate(50,_,Job): `BytesAllocated(Core)==100`, `BytesAllocated(Job)==50`, tag intocada == 0; `Reset()` zera todas |
| 4 | `Memory_IAllocator_PolymorphicAllocate` | `[memory][unit]` | Memory/Tests/Memory_Tag.cpp | `LinearAllocator` visto como `IAllocator&` serve `Allocate` (span não-vazio, alinhado) via vtable |
| 5 | `Memory_Linear_AllocAlignReset` | `[memory][unit]` | Memory/Tests/Memory_Linear.cpp | Allocate com align 1/8/16/64 → `data()` alinhado; blocos não se sobrepõem; `UsedBytes()` cresce; `Free(bloco)` é no-op (`UsedBytes()` inalterado); `Reset()` reusa o endereço base |
| 6 | `Memory_Linear_OOM` | `[memory][unit]` | Memory/Tests/Memory_Linear.cpp | Allocate de `CapacityBytes()+1` → span vazio (`empty()`, `data()==nullptr`); alloc anterior intacta; allocator usável após; OOM não soma em `BytesAllocated` |

**Smoke**: teste #1, < 30 s (HARDENING §7). **Determinismo**: sem RNG/wall-clock; padrão de bytes
literal fixo (ex.: `0xAB`). **Cobertura**: cada símbolo público (`MemoryTag`/`ToString`, `IAllocator`
`Allocate`/`Free`, `LinearAllocator` ctor/`Allocate`/`Free`/`Reset`/`BytesAllocated`/`UsedBytes`/
`CapacityBytes`) aparece em ≥ 1 linha. **TrackingAllocator zero-leak**: N/A nesta task (nasce na T06);
o gate de memória aqui é o preset **asan-debug** (`risco_memoria: true`, HARDENING §12).

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos + gate asan)

```powershell
cmake --preset debug
cmake --preset development
cmake --build --preset debug
cmake --build --preset development
ctest --preset debug --output-on-failure -R "Memory_"
Build\debug\bin\VibeTests.exe "[memory][smoke]" --durations yes
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure

# Gate de memória (risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure -R "Memory_"
```

## Critério de aceitação
- [ ] 6 TEST_CASE do plano verdes — `ctest --preset debug -R "Memory_"`
- [ ] Smoke `Memory_Smoke_Task05` < 1 s — saída de `--durations yes`
- [ ] Suite completa verde em debug e development (não regrediu)
- [ ] **asan-debug verde** — `ctest --preset asan-debug -R "Memory_"` (gate de memória; TrackingAllocator é T06)
- [ ] `new`/`malloc` cru SOMENTE em `LinearAllocator.cpp`; backing liberado no destrutor (RAII) — inspeção
- [ ] Headers Public sem include de Private/, `<windows.h>`, spdlog; `Allocate`/`Free` em `Vspan<Vbyte>` — inspeção
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T05/T06). **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `MemoryTag`, `ToString` | T06, T09, T10, T13 |
| `IAllocator` (`Allocate`/`Free` em `Vspan<Vbyte>`) | T06 (Frame/Tracking derivam), T09, T10, T13 |
| `LinearAllocator` (ctor `CapacityBytes`, `Reset`, `BytesAllocated`) | T09, T10, T13 |

## Hardening aplicável (referência + concretização)
- §3 → aqui: `MemoryTag`/`IAllocator`/`LinearAllocator` PascalCase; membros `m_Base`/`m_Offset`/`m_TagBytes`.
- §4 → aqui: headers em `Public/Memory/`; include `<Memory/Foo.h>`; spdlog/Win32 ausentes.
- §5 → aqui: Doxygen em toda declaração pública.
- §7 → aqui: a tabela do plano É a aplicação; agregação por-instância isola fixtures.
- §12 → aqui: único `new` cru em `LinearAllocator.cpp` (dentro de Memory/); RAII; `Vspan` nas interfaces;
  sem C-cast; **gate asan-debug obrigatório** (risco_memoria).

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Dependências (T02 = Implementado) → 4. Pre-flight
(`Vuint64`/`Vbyte`/`Vspan`/`VASSERT` reais vs Tasks/02 §Contratos) → 5. (git já bootstrapped) →
6. Status Em-execução → 7. Baseline (suite atual verde) → 8. Branch `task/05-memory-tag-allocators` →
9. `tdd` sobre a lista RED → 10. Gate completo + asan-debug → 11. Status Implementado →
12. Commit `[task 05] memory: tag, IAllocator, LinearAllocator`, push, PR.

## Desvios aprovados
(vazio)

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §8.4 · Phases/Fase-01-foundation.md · Decisions/0001, 0003, 0014
