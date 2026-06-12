---
id: 07
nome: platform-windows-base
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [02]
decisions: [0002, 0014, 0021]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: true
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0002-jobsystem-worker-pool.md
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md   # §Contratos entre tasks (T07), §Comandos canônicos
  - Docs/Roadmap/Tasks/01-cmake-vcpkg-presets.md  # apenas §Contratos (build, asan-debug, bin/)
  - Docs/Roadmap/Tasks/02-core-types-handles-result.md  # apenas §Contratos (VResult, Vspan, Vbyte, VASSERT)
files_create:
  - Engine/Source/Runtime/Platform/CMakeLists.txt
  - Engine/Source/Runtime/Platform/Public/Platform/PlatformError.h
  - Engine/Source/Runtime/Platform/Public/Platform/PlatformFile.h
  - Engine/Source/Runtime/Platform/Public/Platform/PlatformThread.h
  - Engine/Source/Runtime/Platform/Public/Platform/PlatformPerformanceCounter.h
  - Engine/Source/Runtime/Platform/Private/Windows/WindowsFile.cpp
  - Engine/Source/Runtime/Platform/Private/Windows/WindowsThread.cpp
  - Engine/Source/Runtime/Platform/Private/Windows/WindowsPerformanceCounter.cpp
  - Engine/Source/Runtime/Platform/Tests/Platform_File.cpp
  - Engine/Source/Runtime/Platform/Tests/Platform_Thread.cpp
  - Engine/Source/Runtime/Platform/Tests/Platform_PerfCounter.cpp
  - Engine/Source/Runtime/Platform/Tests/Platform_Smoke.cpp
files_modify:
  - Engine/Source/Runtime/CMakeLists.txt
  - Engine/Source/Tests/CMakeLists.txt
---

# 07 — Platform: File, Thread, PerformanceCounter (Windows backend)

## Objetivo
Camada de abstração de SO da Fase 1 (design-mvp.md §8.2): `PlatformFile` (I/O síncrono), `PlatformThread`
(backend do `WorkerThread` — ADR 0002) e `PlatformPerformanceCounter` (QPC). Interfaces Public neutras;
backend Windows confinado a `Private/Windows/*.cpp` — **nenhum `<windows.h>` em Public** (PIMPL/handle
opaco `void*`). `Window`/`DynamicLibrary`/`Application` ficam de fora (Application é a T08). O executor NÃO
precisa ler o design.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso**
(HARDENING §14).

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4 (Public/Private), §5, §7, §12, §14 |
| Decisions/0002 | PlatformThread = backend de thread; entry `void(*)(void*)`; affinity no-op MVP | inteiro |
| Decisions/0014 | include `<Platform/Foo.h>` / `<Core/Foo.h>` a partir de Public/ | inteiro |
| Phases/Fase-01 | contrato T07 + comandos (asan) | §Contratos entre tasks (T07), §Comandos canônicos |
| Tasks/01 | preset asan-debug; alvo/teste; bin/ | §Contratos |
| Tasks/02 | `VResult`/`Vspan`/`Vbyte`/`Vuint*`/`VASSERT` | §Contratos |

## Escopo

### Dentro
- 4 headers Public em `Public/Platform/` (1 enum de erro + 3 primitivas), 3 TUs Windows em `Private/Windows/`, 10 testes.
- `PlatformThread::GetPhysicalCoreCount()` (detecção de núcleos físicos para o gate de HW do EngineCore — ADR 0007/0021).
- `PlatformFile` **somente leitura** no MVP; `PlatformThread` move-only; `PlatformPerformanceCounter` estático.

### Fora (NÃO fazer)
- `PlatformApplication` (T08), `Window`, `DynamicLibrary` (Fase 2). Escrita de arquivo (`Write`) — não há cliente Fase 1.
- `<windows.h>`/`<thread>` em qualquer header Public. Allocação dinâmica própria (Platform não é Memory; sem `new`/`malloc`).
- Backend não-Windows (slot multi-backend preservado pela interface, mas só Windows é implementado).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create`/`files_modify` exaustivos. `files_modify`: raiz do Runtime (`add_subdirectory(Platform)`) +
`Engine/Source/Tests/CMakeLists.txt` (linkar `Vibe::Platform` em `VibeTests`; fontes auto-globadas — ADR 0016).
Exceções permanentes: este doc, README, Backlog. Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE quando: (a) contrato não compila vs `<Core/...>` reais; (b) precisar de arquivo fora das listas; (c)
seções se contradizem; (d) teste impossível; (e) gate (incl. asan) falha após 2 ciclos de `diagnose`.
Relate: **passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; `#include <Platform/Foo.h>`. Doxygen §5 completo em TODA decl. pública.)

### Public/Platform/PlatformError.h
```cpp
#include <Core/Types.h>
namespace VibeEngine {
/// @brief Categoria de falha de uma operação de SO (mapeada de GetLastError no backend).
enum class PlatformError : Vuint8 { Unknown, NotFound, AccessDenied, InvalidArgument, IoError };
}
```

### Public/Platform/PlatformFile.h
```cpp
#include <Core/Result.h>
#include <Core/Types.h>
#include <Platform/PlatformError.h>
namespace VibeEngine {
/// @brief Arquivo síncrono RAII (dono de um HANDLE; fecha na destruição). Move-only; somente leitura no MVP.
class PlatformFile {
public:
    PlatformFile() noexcept = default;                  ///< @brief Arquivo fechado (sem handle).
    ~PlatformFile() noexcept;                            ///< @brief Fecha o handle se aberto.
    PlatformFile(const PlatformFile&) = delete;
    PlatformFile& operator=(const PlatformFile&) = delete;
    PlatformFile(PlatformFile&& Other) noexcept;         ///< @brief Move: rouba o handle; Other fica fechado.
    PlatformFile& operator=(PlatformFile&& Other) noexcept; ///< @brief Move-assign (fecha o atual; self-assign seguro).
    /// @brief Abre um arquivo existente para leitura. @param Path caminho nul-terminado @return arquivo aberto ou PlatformError.
    static VResult<PlatformFile, PlatformError> Open(const char* Path) noexcept;
    /// @brief Lê bytes para Dst. @param Dst destino (lê ≤ size()) @return bytes lidos (0=EOF; short read é sucesso) ou erro.
    VResult<Vuint64, PlatformError> Read(Vspan<Vbyte> Dst) noexcept;
    void Close() noexcept;                               ///< @brief Fecha (idempotente; o destrutor também fecha).
    bool IsOpen() const noexcept;                        ///< @brief @return true se há handle aberto.
private:
    void* m_Handle { nullptr };                          ///< @brief HANDLE opaco; nullptr quando fechado.
};
}
```

### Public/Platform/PlatformThread.h
```cpp
#include <Core/Result.h>
#include <Core/Types.h>
#include <Platform/PlatformError.h>
namespace VibeEngine {
/// @brief Thread de SO RAII; backend do WorkerThread (ADR 0002). Move-only. Entry sem captura em heap.
class PlatformThread {
public:
    using EntryFn = void (*)(void* Arg);                 ///< @brief Assinatura da função de entrada.
    PlatformThread() noexcept = default;                 ///< @brief Thread vazia (não joinable).
    ~PlatformThread() noexcept;                          ///< @brief Se ainda joinable: VASSERT + detach (nunca terminate).
    PlatformThread(const PlatformThread&) = delete;
    PlatformThread& operator=(const PlatformThread&) = delete;
    PlatformThread(PlatformThread&& Other) noexcept;     ///< @brief Move: transfere a posse; Other fica não-joinable.
    PlatformThread& operator=(PlatformThread&& Other) noexcept;
    /// @brief Cria e inicia a thread. @param Entry função (não-nula) @param Arg argumento opaco @param Name nome de debug @return thread ou erro.
    static VResult<PlatformThread, PlatformError> Create(EntryFn Entry, void* Arg, Vspan<const char> Name) noexcept;
    void SetName(Vspan<const char> Name) noexcept;       ///< @brief Renomeia (SetThreadDescription). @param Name nome.
    void SetAffinity(Vuint32 CpuIndex) noexcept;         ///< @brief Fixa a um núcleo. @param CpuIndex índice. No-op no MVP (ADR 0002).
    void Join() noexcept;                                 ///< @brief Bloqueia até terminar; depois não-joinable.
    bool IsJoinable() const noexcept;                    ///< @brief @return true se criada e ainda não juntada.
    /// @brief Núcleos físicos da máquina (não lógicos). @return contagem física (>= 1). Usada pelo gate de HW do EngineCore (ADR 0007/0021).
    static Vuint32 GetPhysicalCoreCount() noexcept;
private:
    void* m_Handle { nullptr };                          ///< @brief HANDLE de thread opaco; nullptr quando não-joinable.
};
}
```

### Public/Platform/PlatformPerformanceCounter.h
```cpp
#include <Core/Types.h>
namespace VibeEngine {
/// @brief Acesso estático ao contador monotônico do SO (QueryPerformanceCounter). Primitiva da camada Platform.
class PlatformPerformanceCounter {
public:
    static Vuint64 Now() noexcept;        ///< @brief @return ticks monotônicos crescentes.
    static Vuint64 Frequency() noexcept;  ///< @brief @return ticks por segundo (> 0, constante após boot).
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **Handle = membro `void*`** (não PIMPL/heap) — wrappers de 1 handle; `static_cast<HANDLE>`/`reinterpret_cast` SÓ no `.cpp` (sem C-cast, §12). Sentinela `nullptr` (o backend mapeia `INVALID_HANDLE_VALUE → nullptr` na fronteira). `IsOpen()`/`IsJoinable()` ⇔ `m_Handle != nullptr`.
- **Move-only com sentinela**: o movido-de fica `nullptr`; destrutor no movido-de/fechado é no-op (sem double-close/double-join). Move-assign fecha/junta o atual antes de roubar; self-assign seguro.
- **`PlatformThread` dtor de thread ainda joinable**: `VASSERT` (debug) + `detach` — nunca `std::terminate` (decisão da criação; rede de segurança — o JobSystem T09 junta explicitamente). Entry é `void(*)(void*)`, sem `std::function` (ADR 0002, §13).
- **`PlatformFile::Read`**: nunca escreve além de `Dst.size()` (gate asan de overflow); `bytesRead < size()` é EOF (sucesso). `Dst` vazio → 0. `Open` de arquivo inexistente → `PlatformError::NotFound`.
- **`GetPhysicalCoreCount`** (ADR 0021): `GetLogicalProcessorInformationEx(RelationProcessorCore)` no `WindowsThread.cpp`; conta as entradas `RelationProcessorCore` (núcleos físicos, não lógicos). `noexcept`; em falha improvável retorna `1` (nunca 0). Sem `<windows.h>` em Public.
- **`<windows.h>` SOMENTE em `Private/Windows/*.cpp`**, cada um com `#define WIN32_LEAN_AND_MEAN` e `#define NOMINMAX` no topo. Nenhum `new`/`delete`/`malloc` no módulo (Platform não é Memory; só RAII sobre handle de SO — §12).
- **CMake** (`Platform/CMakeLists.txt`): `add_library(VibePlatform STATIC Private/Windows/Windows{File,Thread,PerformanceCounter}.cpp)` + alias `Vibe::Platform`; `target_include_directories(... PUBLIC Public PRIVATE Private)`; `target_link_libraries(VibePlatform PUBLIC Vibe::Core)`. `Engine/Source/Runtime/CMakeLists.txt` (files_modify) ganha `add_subdirectory(Platform)`; `Engine/Source/Tests/CMakeLists.txt` (files_modify) ganha `Vibe::Platform` no link de `VibeTests` (fontes auto-globadas — ADR 0016).

## Plano de testes (lista RED — ordem de execução do `tdd`)
Os testes criam o arquivo-fixture temporário com a STL (`std::ofstream`) e o leem via `PlatformFile` (mantém `PlatformFile` somente-leitura). Temp file em `std::filesystem::temp_directory_path()`, nome único, apagado no teardown.

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Platform_Smoke_Task07` | `[platform][smoke]` | Platform_Smoke.cpp | escreve temp via ofstream → `PlatformFile::Open`+`Read` em `Vspan<Vbyte>` casa os bytes; `PlatformThread` incrementa um `std::atomic<int>`, `Join`, ==1; `PerfCounter::Now()` 2× monotônico, `Frequency()>0`; sem leak (asan); apaga temp; < 1 s |
| 2 | `Platform_File_OpenReadClose` | `[platform][unit]` | Platform_File.cpp | Open(temp) ok, `IsOpen()`; `Read` casa conteúdo e contagem; após `Close()` `!IsOpen()` |
| 3 | `Platform_File_OpenMissingReturnsError` | `[platform][unit]` | Platform_File.cpp | Open(caminho inexistente) → `!has_value()`, `error()==PlatformError::NotFound`; `!IsOpen()` |
| 4 | `Platform_File_ReadBufferBounds` | `[platform][unit]` | Platform_File.cpp | span maior que o arquivo → `bytesRead < size()` (EOF), sem escrita fora do span; span vazio → 0 |
| 5 | `Platform_File_MoveTransfersOwnership` | `[platform][unit]` | Platform_File.cpp | move-construct: origem `!IsOpen()`, destino `IsOpen()`; sem double-close (asan limpo) |
| 6 | `Platform_Thread_CreateJoinRunsEntry` | `[platform][unit]` | Platform_Thread.cpp | `Create(entry,&arg,name)` (entry seta `std::atomic`); `Join()`; arg observado; `!IsJoinable()` após |
| 7 | `Platform_Thread_SetNameAffinityNoCrash` | `[platform][unit]` | Platform_Thread.cpp | `SetName("VibeWorker")` + `SetAffinity(0)` retornam sem crash; entry ainda roda; `Join` limpo |
| 8 | `Platform_Thread_MoveTransfersOwnership` | `[platform][unit]` | Platform_Thread.cpp | move-construct: origem não-joinable, destino joinable; `Join` via destino; sem double-join |
| 9 | `Platform_PerfCounter_MonotonicFrequency` | `[platform][unit]` | Platform_PerfCounter.cpp | loop fixo: cada `Now() >= ` o anterior (não-decrescente); `Frequency() > 0` e estável entre 2 chamadas |
| 10 | `Platform_Thread_PhysicalCoreCountPositive` | `[platform][unit]` | Platform_Thread.cpp | `GetPhysicalCoreCount() >= 1` e `<= ` núcleos lógicos (`std::thread::hardware_concurrency()`); estável entre 2 chamadas (ADR 0021) |

**Smoke**: teste #1, < 30 s. **Determinismo** (HARDENING §7): sem wall-clock (só monotonia), sem RNG, sem
sleep; sincronização de thread só por `Join`; arquivos temporários únicos apagados no teardown. **Cobertura**:
cada símbolo público dos 4 contratos aparece em ≥ 1 linha. **Gate de memória**: `asan-debug` (`risco_memoria: true`);
vazamento de HANDLE não é pego por asan — é coberto pela asserção de não-double-close/join (#5/#8).

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos + gate asan)

```powershell
cmake --preset debug
cmake --preset development
cmake --build --preset debug
cmake --build --preset development
ctest --preset debug --output-on-failure -R "Platform_"
Build\debug\bin\VibeTests.exe "[platform][smoke]" --durations yes
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure

# Gate de memória (risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure -R "Platform_"
```

## Critério de aceitação
- [ ] 10 TEST_CASE do plano verdes — `ctest --preset debug -R "Platform_"`
- [ ] Smoke `Platform_Smoke_Task07` < 1 s — `--durations yes`
- [ ] Suite completa verde em debug e development (não regrediu)
- [ ] **asan-debug verde** — `ctest --preset asan-debug -R "Platform_"`
- [ ] Headers Public sem `<windows.h>`/`<thread>`/Private/; `<windows.h>` só em `Private/Windows/*.cpp` — inspeção
- [ ] `Read`/`Create` recebem `Vspan`; sem `new`/`malloc`; sem C-cast (só `reinterpret_cast`/`static_cast`) — inspeção
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T07). **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `PlatformError` | T08, T11 |
| `PlatformFile` (`Open`/`Read`/`Close`/`IsOpen`) | T11 (FileSystem) |
| `PlatformThread` (`Create`/`SetName`/`SetAffinity`/`Join`) | T09 (WorkerThread) |
| `PlatformThread::GetPhysicalCoreCount` | T13 (gate de HW do EngineCore — ADR 0007/0021) |
| `PlatformPerformanceCounter` (`Now`/`Frequency`) | T08, T09, T11, T13 |

## Hardening aplicável (referência + concretização)
- §3 → aqui: `PlatformFile`/`PlatformThread`/`PlatformError` PascalCase; `m_Handle`.
- §4 → aqui: Public neutro; `<windows.h>` só em `Private/Windows/*.cpp`; include `<Platform/Foo.h>` (ADR 0014).
- §5 → aqui: Doxygen completo nos 4 headers.
- §7 → aqui: a tabela do plano É a aplicação; fixtures temporários isolados; sincronização por `Join`.
- §12 → aqui: RAII sobre HANDLE; move-only; `Vspan` nas interfaces; sem `new`; sem C-cast; **gate asan-debug**.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Dependências (T02 = Implementado) → 4. Pre-flight
(`VResult`/`Vspan`/`Vbyte`/`VASSERT` reais vs Tasks/02) → 5. (git já bootstrapped) → 6. Status Em-execução →
7. Baseline (suite atual verde) → 8. Branch `task/07-platform-windows-base` → 9. `tdd` sobre a lista RED →
10. Gate completo + asan-debug → 11. Status Implementado → 12. Commit `[task 07] platform: file, thread, perfcounter`, push, PR.

## Desvios aprovados
- **Emenda de criação (ADR 0021, aprovada pelo usuário):** adicionado `PlatformThread::GetPhysicalCoreCount()`
  (+ teste #10) ao contrato. A T13 (EngineCore) precisa da contagem de núcleos físicos para o gate de HW da
  ADR 0007, que a citava mas o contrato original da T07 omitira. Emenda feita com a T07 ainda `Planejado`
  (sem execução retroativa); o registro da fase foi emendado em conjunto.

## Bloqueio
(vazio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §8.2 · Phases/Fase-01-foundation.md · Decisions/0002, 0014, 0021
