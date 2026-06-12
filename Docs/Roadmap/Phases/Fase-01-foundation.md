---
fase: 01
nome: foundation
status: Analisada
fase_anterior: null
fase_proxima: Fase-02-rhi-d3d12
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
decisoes: [0001, 0002, 0003, 0004, 0005, 0006, 0007, 0008, 0009, 0011, 0012, 0013, 0014]
---

# Fase 01 — Foundation

## Objetivo desta fase

Estabelecer a fundação multiplataforma do runtime: tooling de build, tipos fundamentais do `Core`, camada de abstração de plataforma (Windows-only no MVP), allocators básicos, worker pool de jobs e I/O síncrono + assíncrono. Sem esta fase, nenhuma das fases posteriores compila — `RHI` (Fase 2) precisa de `Platform`, `RenderGraph` (Fase 3) precisa de `Memory`, `Renderer` (Fase 4) precisa de `JobSystem`, e assim por diante.

A fase **não entrega nada visual**. O entregável é um executável de teste de console (`VibeTests.exe`) que, ao rodar, demonstra todos os subsistemas operando coordenadamente. Conforme design-mvp.md §9 Fase 1: "executável de teste roda 8 workers, lê arquivo async, loga via spdlog, todos os testes passam".

A intenção é cumprir o §3 acceptance #10 ("compila do zero em ≤ 30 minutos sem intervenção manual") já ao final desta fase, ainda que o produto final do MVP só feche na Fase 12.

## Critério de aceitação da fase

Extraído de [design-mvp.md §9 Fase 1](../../design-mvp.md), refinado pelas ADRs 0001–0007:

```
[ ] Repositório clonado em máquina nova compila VibeTests.exe via CMakePresets + vcpkg em ≤ 30 min
[ ] 4 presets CMake funcionando: debug, development, shipping, asan-debug
[ ] Catch2 com ≥ 30 testes passando, cobrindo 100% das funcionalidades de Core/Math/Memory/JobSystem/FileSystem/Platform
[ ] VibeTests.exe roda smoke [smoke][fase1] em < 30s, com 8 workers, lê 4 KiB async, loga 32 linhas via spdlog
[ ] Naming validado por vx-naming-style em todos os Public/
[ ] Layout Public/Private respeitado (zero include de Private de outros módulos)
[ ] Doxygen tags presentes em todas as declarações públicas
[ ] TrackingAllocator reporta zero leak no shutdown do smoke (Debug e Development)
[ ] vx-hardening-guard retorna OK
[ ] EngineCore::Initialize/Shutdown executa ordem fixa documentada em ADR 0006
[ ] Tracy client compilado, com pelo menos 4 zonas instrumentadas (JobSystem.Schedule, JobSystem.ParallelFor, FileSystem.ReadAsync, WorkerThread.Run)
```

O módulo `EngineCore` está formalizado no layout §6 do design via ADR 0012 (pendência de governança encerrada).

## Critério de aceitação — verificação por máquina

| Critério | Comando/verificação | Evidência esperada |
|---|---|---|
| 4 presets configuram | `cmake --preset debug` / `development` / `shipping` / `asan-debug` | exit 0 em todos |
| Build limpo | `cmake --build --preset debug` e `--preset development` | exit 0, zero warnings (`/W4 /WX`) |
| ≥ 30 testes verdes | `ctest --preset debug --output-on-failure` | "100% tests passed, 0 tests failed out of ≥30" |
| Smoke da fase < 30 s | `Build\debug\bin\VibeTests.exe "[smoke][fase1]" --durations yes` | duração total impressa < 30 s |
| 8 workers + async + log | saída do smoke `[smoke][fase1]` | "workers: 8", leitura async de 4 KiB OK, 32 linhas logadas |
| Zero leak | saída do smoke (Debug e Development) | "TrackingAllocator: 0 leaks" |
| Tracy instrumentado | inspeção: `VPROFILE_ZONE` em JobSystem/FileSystem/WorkerThread | ≥ 4 zonas presentes |

## Stack confirmada para a fase

| Ferramenta | Versão | Onde é adicionada | Fonte |
|---|---|---|---|
| C++23 | MSVC 2022 | `CMakePresets.json` (`/std:c++latest /Zc:__cplusplus`) | design-mvp.md §4 |
| CMake | ≥ 3.28 | `CMakeLists.txt` raiz, `CMakePresets.json` | design-mvp.md §4 |
| Ninja | última | `CMakePresets.json` (`generator: Ninja`) | design-mvp.md §4 |
| vcpkg | manifest, baseline-locked | `vcpkg.json` + `vcpkg-configuration.json` | design-mvp.md §4, §7 |
| glm | última estável | `vcpkg.json` | design-mvp.md §4, §8.3 |
| spdlog | última estável | `vcpkg.json` — **uso síncrono** | design-mvp.md §4, ADR 0006 |
| tracy | última estável | `vcpkg.json` — client lib + macros, **sem overlay** | design-mvp.md §4, ADR 0004 |
| catch2 | v3 | `vcpkg.json` — `Catch2::Catch2WithMain` | design-mvp.md §4, §7, ADR 0005 |

## Módulos e arquivos previstos

**Convenção de include (ADR 0014)**: headers públicos vivem em `Public/<Module>/Foo.h` e são incluídos como `#include <Module/Foo.h>` (ex.: `Core/Public/Core/Types.h` → `#include <Core/Types.h>`). As árvores abaixo omitem o subdiretório `<Module>/` dentro de `Public/` por brevidade; os `files_create` das tasks trazem os caminhos completos.

```
Engine/Source/Runtime/Core/
    Public/
        Types.h            (Vint8..Vuint64, Vfloat, Vdouble, Vbyte, Vspan, Vstring)
        Assert.h           (VASSERT, VVERIFY, VCHECK)
        Logging.h          (VLOG_INFO, VLOG_WARN, VLOG_ERROR)
        Time.h             (FrameTimer, HighResClock)
        StringId.h         (VStringId)
        Handle.h           (VHandle<T> com generation — ADR 0001)
        Result.h           (VResult alias para std::expected — ADR 0001)
        Profile.h          (VPROFILE_ZONE, VPROFILE_FRAME, no-op em Shipping — ADR 0004)
    Private/
        LoggingSink.cpp
        StringIdTable.cpp
        TimeImpl.cpp
    Tests/
        Core_Types.cpp, Core_Handle.cpp, Core_Result.cpp, Core_StringId.cpp,
        Core_Assert.cpp, Core_Logging.cpp

Engine/Source/Runtime/Math/
    Public/
        Vec2.h, Vec3.h, Vec4.h, Mat4.h, Quat.h, Transform.h, MathCommon.h
    Tests/
        Math_Mat4_Identity.cpp, Math_Quat_Normalize.cpp, Math_Common_Lerp.cpp

Engine/Source/Runtime/Memory/
    Public/
        MemoryTag.h        (enum class fechado — ADR 0001)
        IAllocator.h
        LinearAllocator.h
        FrameAllocator.h   (per-thread — ADR 0003)
    Private/
        LinearAllocatorImpl.cpp
        FrameAllocatorImpl.cpp
        TrackingAllocator.cpp  (Debug+Development — ADR 0003)
        MemoryTagRegistry.cpp
    Tests/
        Memory_Linear_AllocAlignReset.cpp, Memory_Linear_OOM.cpp,
        Memory_Frame_RolloverPerFrame.cpp, Memory_Tag_AggregateByCategory.cpp

Engine/Source/Runtime/Platform/
    Public/
        PlatformApplication.h
        PlatformFile.h
        PlatformThread.h
        PlatformPerformanceCounter.h
    Private/Windows/
        WindowsApplication.cpp/.h
        WindowsFile.cpp/.h
        WindowsThread.cpp/.h
        WindowsPerformanceCounter.cpp
    Tests/
        Platform_App_InitShutdown.cpp, Platform_File_OpenReadCloseSync.cpp,
        Platform_Thread_SetNameVisibleInDebugger.cpp,
        Platform_PerfCounter_Monotonic.cpp

Engine/Source/Runtime/JobSystem/
    Public/
        JobSystem.h        (facade)
        JobHandle.h
        JobFence.h
        ParallelFor.h
        TaskGraph.h
    Private/
        WorkerThread.cpp/.h           (sobre PlatformThread — ADR 0002)
        JobQueue.cpp/.h               (work-stealing deque por worker)
        Job.h                         (sizeof == 64, alignas(64) — ADR 0002)
        ParallelForImpl.cpp           (chunks estáticos)
        TaskGraphImpl.cpp
    Tests/
        Job_WorkerPool_SpawnsN.cpp,
        Job_Schedule_RunsOnce.cpp,
        Job_ParallelFor_SumEqualsSerial.cpp,
        Job_TaskGraph_RespectsDependencies.cpp,
        Job_Fence_WaitBlocksUntilCompletion.cpp,
        Job_ParallelFor_NoDataRace.cpp

Engine/Source/Runtime/FileSystem/
    Public/
        FileSystem.h        (facade)
        Path.h
        FileHandle.h
        AsyncReadRequest.h
        FileWatcher.h       (Poll() guard #if VIBE_TESTING — ADR 0005)
    Private/
        FileSystemImpl.cpp
        AsyncReadWorker.cpp
        WindowsFileWatcher.cpp   (ReadDirectoryChangesW)
    Tests/
        FileSystem_ReadSync_SmallFileMatch.cpp,
        FileSystem_ReadAsync_Completes.cpp,
        FileSystem_ReadAsync_CancelBeforeStart.cpp,
        FileSystem_Watcher_DetectsModify.cpp,
        FileSystem_Path_NormalizeSlashes.cpp,
        FileSystem_OpenMissing_ReturnsErr.cpp

Engine/Source/Runtime/EngineCore/
    Public/
        EngineCore.h        (Initialize, Shutdown, EngineCoreConfig — ADR 0006)
    Private/
        EngineCoreImpl.cpp
    Tests/
        EngineCore_Initialize_OrderFixed.cpp,
        EngineCore_Shutdown_ReverseOrder.cpp,
        EngineCore_Smoke_Fase1.cpp   ([smoke][fase1])
```

Arquivos raiz adicionados nesta fase:
```
CMakeLists.txt
CMakePresets.json    (presets: debug, development, shipping, asan-debug)
vcpkg.json
vcpkg-configuration.json
.gitignore
.editorconfig
.clang-format
Scripts/run-tests.ps1
```

## Arquitetura proposta

```mermaid
graph TD
    Core[Core]
    Math[Math]
    Memory[Memory]
    Platform[Platform/Windows]
    JobSystem[JobSystem]
    FileSystem[FileSystem]
    EngineCore[EngineCore]
    VibeTests[VibeTests.exe]

    Math --> Core
    Memory --> Core
    Platform --> Core
    JobSystem --> Core
    JobSystem --> Platform
    JobSystem --> Memory
    FileSystem --> Core
    FileSystem --> Platform
    FileSystem --> JobSystem
    EngineCore --> Core
    EngineCore --> Memory
    EngineCore --> Platform
    EngineCore --> JobSystem
    EngineCore --> FileSystem
    VibeTests --> EngineCore
    VibeTests --> Math
```

Interfaces cross-module relevantes (citando §8.X):
- **Platform → Core** (§8.2): retornos via `VResult<T, PlatformError>`, log via `VLOG_*`, tipos via `Vint*`/`Vbyte`/`Vspan`. Nenhum `<windows.h>` vaza em `Platform/Public/`.
- **JobSystem → Platform** (§5.4, ADR 0002): `WorkerThread` cria threads via `PlatformThread::Create/SetName/SetAffinity`. Slot multi-backend preservado.
- **FileSystem → JobSystem** (§9 Fase 1): `AsyncReadRequest` submete um `Job` cujo `Wait()` é wrapper sobre `JobFence::Wait()`.
- **EngineCore orquestra todos** (ADR 0006, ADR 0012): ordem fixa de Initialize/Shutdown.

## Contratos entre tasks

A superfície pública que cada task EXPÕE para suas sucessoras — registro único de contratos cross-task da fase (HARDENING §14). `vx-task-create` copia daqui para os docs de task; qualquer emenda começa AQUI e se propaga (ver Protocolo de revisão de contratos). Somente símbolos consumidos por OUTRAS tasks; superfície interna fica no doc de cada task.

### T01 — cmake-vcpkg-presets
Não expõe símbolos C++. Expõe convenções de build consumidas por todas as tasks (ADR 0008):
presets `debug`/`development`/`shipping`/`asan-debug`; alvos `Vibe<Module>` com alias `Vibe::<Module>`; executáveis em `Build/<preset>/bin/` (`CMAKE_RUNTIME_OUTPUT_DIRECTORY`); defines `VIBE_*` propagados PUBLIC.
Consumido por: T02–T14.

### T02 — core-types-handles-result
Expõe (`Engine/Source/Runtime/Core/Public/`):
```cpp
namespace VibeEngine {
using Vint8 = std::int8_t;   using Vuint8  = std::uint8_t;   // ... até Vint64/Vuint64
using Vfloat = float;        using Vdouble = double;         using Vbyte = std::byte;
template <typename T> using Vspan = std::span<T>;
using Vstring = std::string;                                  // ADR 0001
template <typename T, typename E> using VResult = std::expected<T, E>;  // ADR 0001
template <typename T> class VHandle; // 8 B: m_Index + m_Generation (Vuint32); Invalid(), Index(), Generation(), IsValid(), operator== (ADR 0001)
class VStringId;              // FNV-1a 64-bit sem seed; Value(), DebugString(); operator""_sid (ADR 0009)
}
#define VASSERT(Expr, ...)    // no-op em Shipping; handler de teste via VASSERT_SetHandlerForTesting (ADR 0009)
#define VVERIFY(Expr, ...)    // mantém avaliação em Shipping
#define VCHECK(Expr, ...)     // ativo em todos os builds
```
Consumido por: T03–T14 (todas).

### T03 — core-logging-time-profile
Expõe (`Engine/Source/Runtime/Core/Public/`):
```cpp
#define VLOG_INFO(...)  /* spdlog síncrono; ((void)0) em Shipping (ADR 0006) */
#define VLOG_WARN(...)
#define VLOG_ERROR(...)
namespace VibeEngine {
class HighResClock;   // static Vuint64 NowTicks(); static Vdouble TicksToSeconds(Vuint64)
class FrameTimer;     // void Tick(); Vdouble DeltaSeconds() const; Vuint64 FrameIndex() const
}
#define VPROFILE_ZONE(Name)   // Tracy; no-op sem TRACY_ENABLE (ADR 0004)
#define VPROFILE_FRAME()
#define VPROFILE_PLOT(Name, Value)
```
Consumido por: T05–T14.

### T04 — math-wrappers-glm
Expõe (`Engine/Source/Runtime/Math/Public/`, ADR 0017):
```cpp
namespace VibeEngine {                       // tipos no namespace raiz (ADR 0017)
using Vec2 = glm::vec2; using Vec3 = glm::vec3; using Vec4 = glm::vec4;
using Mat4 = glm::mat4; using Quat = glm::quat;
struct Transform { Vec3 m_Position; Quat m_Rotation; Vec3 m_Scale; };
}
namespace VibeEngine::Math {                  // operações como free functions (ADR 0017)
inline constexpr Vfloat Epsilon = 1e-6f;
Mat4 Identity(); Quat Normalize(const Quat&); Vfloat Length(const Quat&);
Vec3 Lerp(const Vec3&, const Vec3&, Vfloat);  // superfície mínima MVP; cresce just-in-time
}
```
Consumido por: fases 2+ (Renderer, Animation, Character, Camera). Não consumido dentro da Fase 1.

### T05/T06 — memory
Expõe (`Engine/Source/Runtime/Memory/Public/Memory/`, ADR 0014/0001/0003). Assinaturas refinadas
na criação da T05 (Allocate/Free em `Vspan<Vbyte>` por HARDENING §12; LinearAllocator possui o
backing; agregação por-tag por instância):
```cpp
namespace VibeEngine {
enum class MemoryTag : Vuint16 { Core, Job, FileSystem, Frame, Debug, Count /* fechado — ADR 0001 */ };
const char* ToString(MemoryTag) noexcept;
class IAllocator {                            // interface multi-backend
  virtual Vspan<Vbyte> Allocate(Vuint64 Size, Vuint64 Alignment, MemoryTag) noexcept = 0;
  virtual void Free(Vspan<Vbyte> Block) noexcept = 0;   // span vazio = OOM/no-op
};
class LinearAllocator;  // : IAllocator; possui buffer (CapacityBytes ctor); Reset(); BytesAllocated(MemoryTag) — T05
class FrameAllocator;   // : IAllocator; per-thread, possui chunks; BeginFrame(); grow+VLOG_WARN; ChunkCount() — T06
class TrackingAllocator;// : IAllocator; wrap de IAllocator&; LiveBytes(MemoryTag)/LiveAllocationCount()/ReportLeaks(); #if VIBE_TRACKING_ALLOC — T06
}
```
Consumido por: T09, T10, T13, T14 (TrackingAllocator é o mecanismo do gate zero-leak do smoke da fase).

### T07 — platform-windows-base
Expõe (`Engine/Source/Runtime/Platform/Public/Platform/`, ADR 0014; backend Win32 confinado a Private/Windows; sem `<windows.h>` em Public):
```cpp
namespace VibeEngine {
enum class PlatformError : Vuint8 { Unknown, NotFound, AccessDenied, InvalidArgument, IoError };
class PlatformFile {               // RAII sobre HANDLE (void*); move-only; somente leitura no MVP
  static VResult<PlatformFile, PlatformError> Open(const char* Path) noexcept;
  VResult<Vuint64, PlatformError> Read(Vspan<Vbyte> Dst) noexcept;  // bytes lidos; short read = EOF
  void Close() noexcept; bool IsOpen() const noexcept; };
class PlatformThread {             // RAII; move-only; backend do WorkerThread (ADR 0002)
  using EntryFn = void(*)(void*);
  static VResult<PlatformThread, PlatformError> Create(EntryFn, void* Arg, Vspan<const char> Name) noexcept;
  void SetName(Vspan<const char>) noexcept; void SetAffinity(Vuint32 CpuIndex) noexcept;  // affinity no-op MVP
  void Join() noexcept; bool IsJoinable() const noexcept;
  static Vuint32 GetPhysicalCoreCount() noexcept; };  // GetLogicalProcessorInformationEx(RelationProcessorCore) — ADR 0007/0021
class PlatformPerformanceCounter { static Vuint64 Now() noexcept; static Vuint64 Frequency() noexcept; };
}
```
Consumido por: T08, T09 (WorkerThread usa PlatformThread), T11 (FileSystem usa PlatformFile), T13 (GetPhysicalCoreCount — ADR 0007/0021).

### T08 — platform-windows-application
Expõe (`Engine/Source/Runtime/Platform/Public/Platform/`):
```cpp
namespace VibeEngine {
enum class PumpResult : Vuint8 { Continue, QuitRequested };
class PlatformApplication {        // lifecycle headless (sem janela na Fase 1); message pump Win32 em PImpl
  VResult<void, PlatformError> Initialize();   // idempotente
  PumpResult PumpMessages();                    // não-bloqueante (PeekMessage); drena 1 passada
  void RequestQuit(); void Shutdown(); bool IsInitialized() const; };
}
```
Consumido por: T13, T14 (main loop do VibeGame/Editor entra na Fase 12).

### T09/T10 — jobsystem
Expõe (`Engine/Source/Runtime/JobSystem/Public/JobSystem/`, namespace raiz `VibeEngine`; alvo
`VibeJobSystem`/alias `Vibe::JobSystem`; superfície elaborada e fronteira T09|T10 fixadas por **ADR 0018**):
```cpp
namespace VibeEngine {
// ---- T09 (worker-queue): JobHandle, JobFence, facade JobSystem ----
struct JobHandle { Vuint32 m_Index; Vuint32 m_Generation; bool IsValid() const noexcept; }; // 8 B, POD opaco (ADR 0001/0002)
class JobFence {                        // move-only; void Wait() noexcept (cooperativo); bool IsComplete() const noexcept
};
class JobSystem {                       // facade ESTÁTICO; pool global (heap frio); Job 64 B, payload inline 48 B (ADR 0002/0018)
  static void Initialize(Vuint32 WorkerCount);   // ordem fixa via EngineCore (ADR 0006/0007)
  static void Shutdown();  static bool IsInitialized() noexcept;  static Vuint32 WorkerCount() noexcept;
  template <class Fn> static JobHandle Schedule(Fn&&) noexcept;        // captura ≤ 48 B (static_assert, ADR 0002)
  template <class Fn> static void Schedule(Fn&&, JobFence&) noexcept;  // associa job à fence
  static void Wait(JobHandle) noexcept;  static bool IsComplete(JobHandle) noexcept;
};
// ---- T10 (graph-parallelfor): ParallelFor, TaskGraph ----
template <class Fn> void ParallelFor(Vuint32 Count, Vuint32 ChunkSize, Fn&& Body) noexcept; // Body por intervalo void(Begin,End); ChunkSize 0 = split estático
class TaskGraph {                       // single-shot; storage via IAllocator& (MemoryTag::Job, ADR 0018)
  using NodeId = Vuint32;
  explicit TaskGraph(IAllocator&) noexcept;
  template <class Fn> NodeId AddNode(Fn&&) noexcept;   // mesma guarda de 48 B
  void AddDependency(NodeId Before, NodeId After) noexcept;
  JobFence Submit() noexcept;           // agenda o grafo respeitando dependências; consome o grafo
};
}
```
Notas de fronteira (ADR 0018): **T09** entrega `JobHandle`/`JobFence`/facade `JobSystem` (linka só
`Vibe::Core`+`Vibe::Platform`; pool em heap frio `unique_ptr`, §12); **T10** entrega `ParallelFor`+`TaskGraph`
(adiciona o link `Vibe::Memory` ao alvo). `JobFence` é construída por associação em T09 e reusada por T10.
Consumido por: T12, T13, T14.

### T11/T12 — filesystem
Expõe (`Engine/Source/Runtime/FileSystem/Public/FileSystem/`, alvo `VibeFileSystem`/alias
`Vibe::FileSystem`; superfície elaborada por **ADR 0020**; sem `<windows.h>` em Public):
```cpp
namespace VibeEngine {
// ---- T11 (sync-path): Path, FileError, FileSystem::ReadSync ----
enum class FileError : Vuint8 { Unknown, NotFound, AccessDenied, InvalidArgument, IoError, Cancelled };
class Path {            // value type; ctor normaliza '\\'→'/'; Join, Extension (".gltf" c/ ponto), Str(), CStr()
};
class FileSystem {      // fachada estática
  static VResult<std::vector<Vbyte>, FileError> ReadSync(const Path&);              // buffer próprio
  static VResult<Vuint64, FileError> ReadSync(const Path&, Vspan<Vbyte> Dst);       // zero-alloc (ADR 0020); >Dst = InvalidArgument
  static AsyncReadRequest ReadAsync(const Path&);                                    // T12; VPROFILE_ZONE("FileSystem.ReadAsync")
};
// ---- T12 (async-watcher): AsyncReadRequest, FileWatcher ----
class AsyncReadRequest { // move-only; dono de unique_ptr<ReadState> (buffer+JobFence+status atômico); job captura ReadState* 8B (ADR 0002)
  void Wait() noexcept; bool Cancel() noexcept; VResult<std::vector<Vbyte>, FileError> Result(); // dtor faz Wait() — sem UAF
};
using FileWatchCallback = void (*)(const Path& ChangedPath, void* UserData);        // sem std::function (§13)
class FileWatcher {     // ReadDirectoryChangesW+OVERLAPPED em Private; Poll-unificado (ADR 0020, refina ADR 0005)
  VResult<void, FileError> Watch(const Path& DirPath, FileWatchCallback, void* UserData);
  void Poll();          // drena eventos e dispara callbacks na thread do chamador (incondicional — ADR 0020)
};
}
```
Notas de fronteira (ADR 0020): **T11** entrega `Path`/`FileError`/`FileSystem::ReadSync` (linka
`Vibe::Core`+`Vibe::Platform`); **T12** entrega `AsyncReadRequest`/`ReadAsync`/`FileWatcher` (adiciona o link
`Vibe::JobSystem`; modifica `FileSystem.h` para `ReadAsync`). `Poll()` é incondicional (drain por frame em
produção, drain nos testes) — **refina** o `Poll() #if VIBE_TESTING` do ADR 0005; sem thread dedicada de watcher.
Consumido por: T13, T14.

### T13 — engine-core-bootstrap
Expõe (`Engine/Source/Runtime/EngineCore/Public/EngineCore/`, alvo `VibeEngineCore`/alias `Vibe::EngineCore`,
módulo próprio — ADR 0012; forma `class EngineCore` fixada por **ADR 0021**):
```cpp
namespace VibeEngine {
enum class EngineError : Vuint32 { Unknown, AlreadyInitialized, InsufficientHardware, SubsystemInitFailed }; // ADR 0021
struct EngineCoreConfig { Vuint32 m_WorkerCount{0}; Vuint64 m_FrameAllocatorSize{0}; bool m_EnableTracking{true}; }; // ADR 0006
class EngineCore {                  // facade estático (ADR 0021); orquestra ordem fixa (ADR 0006)
  EngineCore() = delete;
  static VResult<void, EngineError> Initialize(const EngineCoreConfig& Config); // <8 físicos em auto → InsufficientHardware (ADR 0007)
  static void Shutdown();           // ordem inversa; passo Memory loga "TrackingAllocator: <N> leaks" (ADR 0021)
};
}
```
Notas (ADR 0021): linka `Vibe::Core`+`Vibe::Memory`+`Vibe::Platform`+`Vibe::JobSystem` (NÃO `Vibe::FileSystem` —
stateless na Fase 1, passo só marcador de log). Possui system `LinearAllocator`+`TrackingAllocator` (opt-in) +
`FrameAllocator` por worker. Cada passo de boot/teardown emite `VLOG_INFO` marcador (ordem observável nos testes).
Consumido por: T14.

## Comandos canônicos da fase

Bloco único — tasks copiam DAQUI. Preset/caminho mudou → atualizar aqui primeiro, depois rebake das tasks ainda `Planejado` (HARDENING §14). Status: **previsto** (vira **verificado** quando a T01 aterrissar e os caminhos forem confirmados).

```powershell
# Configurar (uma vez por preset)
cmake --preset debug
cmake --preset development

# Build (gate: /W4 /WX falham com warning novo — ADR 0008)
cmake --build --preset debug
cmake --build --preset development

# Suite completa
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure

# Smoke da fase (com medição de duração)
Build\debug\bin\VibeTests.exe "[smoke][fase1]" --durations yes

# Variantes por task (o doc da task define o filtro -R e as tags; a forma é esta):
ctest --preset debug --output-on-failure -R "<FiltroDaTask>"
Build\debug\bin\VibeTests.exe "[smoke][<modulo>]" --durations yes

# Gate de memória (apenas tasks com risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure
```

## Ordem interna de implementação

Os passos abaixo viram um conjunto de tasks via `vx-task-create` (esboço em "Tasks previstas"):

1. **Setup CMake/vcpkg/presets** — `CMakeLists.txt` raiz, `CMakePresets.json` com 4 presets, `vcpkg.json` baseline-locked, `.gitignore`, `.editorconfig`, `.clang-format`. Critério: `cmake --preset debug && cmake --build --preset debug` produz binário vazio.
2. **Core** — Types → Assert → Logging (síncrono) → Time → StringId → Handle (8 B com generation) → Result (alias `std::expected`) → Profile (macros Tracy). Critério: testes [core] passam.
3. **Math** — wrappers triviais sobre glm. Critério: 3 testes [math] passam.
4. **Memory** — `MemoryTag` enum → `IAllocator` → `LinearAllocator` → `FrameAllocator` (per-thread) → `TrackingAllocator` (wrapper Debug+Development). Critério: 4 testes [memory] passam, smoke do tracking reporta zero leak.
5. **Platform/Windows** — interfaces Public primeiro; depois Windows backend de `File` → `Thread` → `PerformanceCounter` → `Application`. Critério: 4 testes [platform] passam.
6. **JobSystem** — `WorkerThread` (sobre PlatformThread) → `Job` (64 B aligned, ADR 0002) → `JobQueue` (work-stealing deque com head/tail em cache lines separadas) → `JobHandle` → `JobFence` → `ParallelFor` (chunks estáticos) → `TaskGraph` (predecessors atomic). Critério: 6 testes [job] passam, sem flakiness em 100 execuções consecutivas.
7. **FileSystem** — `Path` → `FileHandle` síncrono (via PlatformFile) → `AsyncReadRequest` (via JobSystem) → `FileWatcher` (ReadDirectoryChangesW) com `Poll()` guard `#if VIBE_TESTING`. Critério: 6 testes [filesystem] passam.
8. **EngineCore::Initialize/Shutdown** — orquestrador central com ordem fixa (ADR 0006). Critério: 2 testes de ordem passam.
9. **VibeTests.exe + smoke global** — alvo CMake único, `catch_discover_tests()`, `Scripts/run-tests.ps1`. `EngineCore_Smoke_Fase1.cpp` reproduz literalmente o critério §9 Fase 1. Critério: smoke roda em < 5 s; total da suite ≤ 60 s.

## Tasks previstas (esboço)

```
01-cmake-vcpkg-presets             — Setup de build (CMake + Ninja + vcpkg + 4 presets)
02-core-types-handles-result       — Core: Types, Handle, Result, StringId, Assert
03-core-logging-time-profile       — Core: Logging síncrono, Time, Profile (Tracy macros)
04-math-wrappers-glm               — Math wrappers finos sobre glm
05-memory-tag-allocators           — MemoryTag, IAllocator, LinearAllocator
06-memory-frame-tracking           — FrameAllocator per-thread + TrackingAllocator
07-platform-windows-base           — Platform: File, Thread, PerformanceCounter (Public + Windows backend)
08-platform-windows-application    — Platform: Application (loop, lifetime)
09-jobsystem-worker-queue          — JobSystem: WorkerThread, Job 64B, JobQueue work-stealing
10-jobsystem-graph-parallelfor     — JobSystem: TaskGraph, ParallelFor, JobFence
11-filesystem-sync-path            — FileSystem: Path, FileHandle síncrono
12-filesystem-async-watcher        — FileSystem: AsyncReadRequest + FileWatcher
13-engine-core-bootstrap           — EngineCore::Initialize/Shutdown com ordem fixa
14-vibetests-exe-smoke-fase1       — VibeTests.exe + smoke global da Fase 1
```

Cada uma vai a `vx-task-create` para virar `Docs/Roadmap/Tasks/NN-nome.md` no **formato 2** (ADR 0013): especialistas são consultados na CRIAÇÃO e seus contratos assados no doc; a execução é mecânica. Norma operacional: criar 1–3 tasks à frente da execução, não as 14 de uma vez (HARDENING §14).

## Flags de risco por task

Fonte: recomendação do `vx-spec-memory-perf` (HARDENING §12/§13). `vx-task-create` copia para o frontmatter.

| Task | risco_memoria | risco_frame | Gate extra |
|---|---|---|---|
| 01-cmake-vcpkg-presets | false | false | — |
| 02-core-types-handles-result | false | false | — |
| 03-core-logging-time-profile | false | false | — |
| 04-math-wrappers-glm | false | false | — |
| 05-memory-tag-allocators | **true** | false | asan-debug + zero-leak |
| 06-memory-frame-tracking | **true** | false | asan-debug + zero-leak |
| 07-platform-windows-base | **true** | false | asan-debug (handles Win32, buffers) |
| 08-platform-windows-application | false | false | — |
| 09-jobsystem-worker-queue | **true** | **true** | asan-debug + teste [perf] (deque work-stealing) |
| 10-jobsystem-graph-parallelfor | **true** | **true** | asan-debug + teste [perf] (ParallelFor escala — §11) |
| 11-filesystem-sync-path | **true** | false | asan-debug (buffers de leitura) |
| 12-filesystem-async-watcher | **true** | false | asan-debug (OVERLAPPED, lifetime de request) |
| 13-engine-core-bootstrap | false | false | — (smoke já cobre zero-leak global) |
| 14-vibetests-exe-smoke-fase1 | false | false | — |

## Protocolo de revisão de contratos (churn — HARDENING §14)

Quando um Desvio aprovado em execução muda um símbolo de "Contratos entre tasks":

1. Emendar a seção **Contratos entre tasks** desta fase.
2. Listar tasks afetadas via `vx-doc-graph` ("dependentes de NN").
3. Rebake de cada task afetada ainda `Planejado` via `vx-task-create`.
4. Tasks afetadas já `Implementado` → nova task corretiva, nunca edição retroativa.

## Riscos específicos da fase

Referencia design-mvp.md §12 mais riscos levantados pelos especialistas:

| Risco | Mitigação | Origem |
|---|---|---|
| Platform vazando `<windows.h>` em Public | PIMPL ou `void*` typed handle; revisão por `vx-naming-style` em cada PR | vx-spec-architecture |
| `JobQueue` sem padding cache-line → false sharing letal na Fase 4 | `alignas(64)` em head/tail de cada deque desde o início (ADR 0002) | vx-spec-memory-perf |
| `std::function` infiltrado como payload de Job | Compile-time check `sizeof(captures) ≤ 48` (ADR 0002) | vx-spec-memory-perf |
| FileWatcher flaky em CI lento | `Poll()` síncrono guard `#if VIBE_TESTING` (ADR 0005) | vx-spec-testing |
| Logging morrendo antes do JobSystem | Ordem reversa de Shutdown fixada (ADR 0006) | vx-spec-architecture |
| `TrackingAllocator` sub-relatando (não cobre `new` global) | Aceito — opt-in via allocators da engine (ADR 0003) | vx-spec-memory-perf |
| Tracy só ativado na Fase 3 → retrofit cross-cutting | Integrado já agora (ADR 0004) | vx-spec-memory-perf |
| `glm` exposto em headers Public — troca futura cara | Typedefs com prefixo V; operações via funções livres em `VibeEngine::Math` | vx-spec-architecture |
| HW menor que 8 cores físicos → smoke falha startup | Mensagem clara apontando ADR 0007 + override `EngineCoreConfig::m_WorkerCount` | vx-spec-memory-perf |

## Hardening relevante

Aplicam-se com força nesta fase:

- **§1 Escopo do MVP**: nada fora da lista in-scope. `Window` e `DynamicLibrary` saem do Platform desta fase (não há cliente in-scope da Fase 1).
- **§2 Lista negativa**: nenhum item da lista é tocado. Plugin system (motivação clássica para `MemoryTag` aberto e `DynamicLibrary`) está banido — ADR 0001 e remoção de DL refletem isso.
- **§3 Nomenclatura**: V-prefix em todos os tipos fundamentais; `m_PascalCase` em membros; `PascalCase` em funções/classes/arquivos; `VLOG_*`/`VASSERT` em SCREAMING_SNAKE.
- **§4 Public/Private**: cada módulo expõe apenas Public; testes em pasta `Tests/` separada.
- **§5 Doxygen**: obrigatório em toda declaração Public dos 6 módulos.
- **§7 Testes**: ≥ 30 testes Catch2 (excede o piso §9 de 20+) cobrindo 100% das funcionalidades; smoke < 30 s; determinístico (zero `sleep_for`, zero RNG sem seed); fixtures isolados; tags por módulo.
- **§8 Regra de decisão**: ADRs 0001–0009 + 0011–0013 cobrem as escolhas arquiteturais e de processo da fase. Qualquer nova escolha vira ADR antes da task.
- **§10 Critério de tarefa pronta**: cada uma das 14 tasks previstas deverá satisfazer integralmente a checklist do §10 (incluindo os itens novos de §12/§13/§14).
- **§12 Segurança de memória**: RAII em tudo; allocators com `MemoryTag`; zero leaks no smoke; asan-debug nas tasks flagadas (ver Flags de risco por task).
- **§13 Orçamento de performance**: Tracy zones desde a T03 (ADR 0004); testes `[perf]` nas tasks de JobSystem (T09/T10 — "ParallelFor escala em throughput", §11).
- **§14 Contrato de execução (formato 2)**: todas as tasks desta fase são criadas no formato 2; tasks v1 antigas exigem rebake antes de executar.

## Perguntas em aberto

Nenhuma. Todas as 14 decisões surfacing surgidas dos especialistas foram respondidas pelo usuário e registradas em ADRs 0001–0007 (e ajustes Karpathy aceitos: 4 presets em vez de 6, spdlog síncrono em vez de async, Platform reduzido, Math com 3 testes).

A fase está pronta para ser desdobrada em tasks via `vx-task-create`.
