---
id: 11
nome: filesystem-sync-path
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [07]
decisions: [0014, 0020]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: true
risco_frame: false
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Decisions/0020-filesystem-leitura-e-watcher.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md          # §Contratos entre tasks (T11/T12), §Comandos canônicos
  - Docs/Roadmap/Tasks/02-core-types-handles-result.md # apenas §Contratos (Vbyte/Vspan/Vstring/VResult/Vuint64)
  - Docs/Roadmap/Tasks/07-platform-windows-base.md     # apenas §Contratos (PlatformFile, PlatformError)
files_create:
  - Engine/Source/Runtime/FileSystem/CMakeLists.txt
  - Engine/Source/Runtime/FileSystem/Public/FileSystem/FileError.h
  - Engine/Source/Runtime/FileSystem/Public/FileSystem/Path.h
  - Engine/Source/Runtime/FileSystem/Public/FileSystem/FileSystem.h
  - Engine/Source/Runtime/FileSystem/Private/Path.cpp
  - Engine/Source/Runtime/FileSystem/Private/FileSystem.cpp
  - Engine/Source/Runtime/FileSystem/Tests/FileSystem_Path.cpp
  - Engine/Source/Runtime/FileSystem/Tests/FileSystem_ReadSync.cpp
files_modify:
  - Engine/Source/Runtime/CMakeLists.txt
  - Engine/Source/Tests/CMakeLists.txt
---

# 11 — FileSystem: Path + ReadSync síncrono

## Objetivo
Primeira fatia do módulo `FileSystem` (design-mvp.md §9 "read sync"): `Path` (value type que normaliza
barras), `FileError` (enum mapeado de `PlatformError`) e a fachada `FileSystem::ReadSync` em duas formas —
buffer próprio (`std::vector<Vbyte>`) e zero-alloc para um `Vspan` do chamador (ADR 0020). Leitura via
`PlatformFile` (T07). Async e watcher são a T12. O executor NÃO precisa ler o design — os contratos abaixo
são completos.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso**
(HARDENING §14). T07 ainda não está implementada na criação → o pre-flight compara os contratos contra
`Tasks/07 §Contratos`; ao executar, o gate de dependências garante os headers reais.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4, §5, §7, §12, §14 |
| Decisions/0014 | include `<FileSystem/Foo.h>` / `<Core/Foo.h>`; Public/<Module>/ | inteiro |
| Decisions/0020 | duas formas de ReadSync; FileError com Cancelled; FileSystem não linka Memory | inteiro |
| Phases/Fase-01 | contrato T11/T12 + comandos (asan) | §Contratos entre tasks, §Comandos canônicos |
| Tasks/02 | `Vbyte`/`Vspan`/`Vstring`/`Vuint64`/`VResult` | §Contratos |
| Tasks/07 | `PlatformFile` (Open/Read/Close/IsOpen), `PlatformError` | §Contratos |

## Escopo

### Dentro
- Alvo `Vibe::FileSystem` (STATIC) com 3 headers Public em `Public/FileSystem/`, 2 TUs Private, 2 arquivos de teste.
- `Path`: ctor normaliza `\` → `/`; `Join` (1 separador), `Extension` (com ponto), `Str()`, `CStr()`.
- `FileSystem::ReadSync` em 2 sobrecargas (buffer próprio e `Vspan` zero-alloc — ADR 0020).
- `FileError` enum mapeado de `PlatformError` na fronteira (tradução em Private).

### Fora (NÃO fazer, mesmo que pareça óbvio)
- `ReadAsync`, `AsyncReadRequest`, `FileWatcher` (T12). Link de `Vibe::JobSystem` (entra na T12).
- `FileHandle` público de streaming (registro NÃO o expõe — ADR 0020; o MVP carrega arquivos inteiros).
- Link de `Vibe::Memory` (FileSystem não depende de Memory — mermaid da fase). Escrita de arquivo.
- `<windows.h>` em qualquer header (Path é UTF-8 puro; conversão wide é detalhe Private, mas T11 nem precisa — usa `PlatformFile`).

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create`/`files_modify` exaustivos. `files_modify`: raiz do Runtime (`add_subdirectory(FileSystem)`) +
`Engine/Source/Tests/CMakeLists.txt` (linkar `Vibe::FileSystem` em `VibeTests`; fontes auto-globadas — ADR 0016).
Exceções permanentes: este doc, README, Backlog. Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) contrato não compila vs `<Platform/PlatformFile.h>`/`<Core/...>`
reais; (b) precisar de arquivo fora das listas; (c) seções se contradizem; (d) teste impossível; (e) gate
(incl. asan) falha após 2 ciclos de `diagnose`. Relate: **passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <FileSystem/Foo.h>`. Doxygen §5 completo.)

### Public/FileSystem/FileError.h
```cpp
#include <Core/Types.h>   // Vuint8

namespace VibeEngine
{
/// @brief Erro de operação de filesystem; espelha PlatformError (T07) + `Cancelled` para leituras async (ADR 0020).
enum class FileError : Vuint8 { Unknown, NotFound, AccessDenied, InvalidArgument, IoError, Cancelled };
}
```

### Public/FileSystem/Path.h
```cpp
#include <Core/Types.h>   // Vstring

namespace VibeEngine
{
/**
 * @brief Caminho de arquivo UTF-8 (value type, copiável). Invariante: barras normalizadas para '/'.
 * @details Sem I/O e sem `<windows.h>`; é um wrapper fino sobre `Vstring`. Cold path.
 */
class Path
{
public:
    Path() = default;                                   ///< @brief Caminho vazio.
    /// @brief Constrói de C-string UTF-8; normaliza '\\' → '/'. @param Utf8 caminho nul-terminado.
    explicit Path(const char* Utf8);
    /// @brief Constrói de Vstring UTF-8 (cópia); normaliza '\\' → '/'. @param Utf8 caminho.
    explicit Path(Vstring Utf8);
    /// @brief Concatena com um separador '/' único (sem duplicar). @param Other sufixo. @return caminho juntado.
    Path Join(const Path& Other) const;
    /// @brief Extensão incluindo o ponto (ex.: ".gltf"); vazia se não houver. @return extensão.
    Vstring Extension() const;
    /// @brief String normalizada subjacente. @return referência válida enquanto o Path viver.
    const Vstring& Str() const noexcept;
    /// @brief Ponteiro C nul-terminado (para PlatformFile::Open). @return c_str() do buffer interno.
    const char* CStr() const noexcept;

private:
    Vstring m_Value;   ///< @brief Caminho UTF-8 normalizado ('/').
};
}
```

### Public/FileSystem/FileSystem.h
```cpp
#include <Core/Result.h>   // VResult
#include <Core/Types.h>    // Vbyte, Vspan, Vuint64
#include <FileSystem/FileError.h>
#include <FileSystem/Path.h>

#include <vector>

namespace VibeEngine
{
/// @brief Fachada estática de I/O de arquivos (cold path). Sem estado. (ReadAsync entra na T12.)
class FileSystem
{
public:
    /**
     * @brief Lê um arquivo inteiro para um buffer próprio.
     * @param FilePath Caminho do arquivo.
     * @return Bytes do arquivo (`std::vector<Vbyte>`), ou `FileError` (NotFound/AccessDenied/IoError).
     */
    static VResult<std::vector<Vbyte>, FileError> ReadSync(const Path& FilePath);

    /**
     * @brief Lê para um buffer fornecido pelo chamador (zero-alloc — forma §12-preferida, ADR 0020).
     * @param FilePath Caminho do arquivo.
     * @param Dst Buffer de destino (caller-owned); recebe `Vspan`, nunca ptr+size.
     * @return Bytes lidos; `FileError::InvalidArgument` se o arquivo for maior que `Dst.size()` (sem truncar).
     */
    static VResult<Vuint64, FileError> ReadSync(const Path& FilePath, Vspan<Vbyte> Dst);
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **`Path` normaliza no ctor** (invariante sempre-normalizado): substitui cada `\` por `/`. `Join`: se a base
  termina em `/`, não duplica; senão insere um `/`. `Extension`: do último `.` após a última `/`; INCLUI o
  ponto (`".gltf"`); sem ponto → `Vstring{}` vazia (convenção std::filesystem — ADR 0020). `Str()`/`CStr()`
  expõem o buffer interno (não-dono). Sem `<windows.h>`.
- **`FileError` espelha `PlatformError`** (T07) 1:1 nos comuns; a tradução `PlatformError → FileError` vive
  em `FileSystem.cpp` (Private) — o módulo NÃO vaza tipos de Platform na fronteira. `Cancelled` não é usado
  em T11 (é da leitura async, T12); existe no enum desde já (ADR 0020).
- **`ReadSync(Path)`**: abre via `PlatformFile::Open(Path::CStr())`; lê em laço para um `std::vector<Vbyte>`
  que cresce (sem `PlatformFile::Size()` no contrato T07 — lê até `Read` retornar 0 = EOF). `std::vector` é
  cold path, permitido §12; sai por move via `VResult`. Open inexistente → `FileError::NotFound`.
- **`ReadSync(Path, Vspan<Vbyte> Dst)`**: lê até `Dst.size()` bytes; se o arquivo exceder `Dst.size()`
  (uma leitura-sonda de 1 byte após preencher `Dst` retorna byte) → `FileError::InvalidArgument` (sem
  truncamento silencioso — ADR 0020). Retorna a contagem de bytes lidos. `Dst` vazio + arquivo não-vazio → InvalidArgument.
- **Sem `new`/`malloc` cru** (FileSystem não é Memory): só `std::vector`/RAII em código frio (§12). Sem C-cast.
- **CMake** (`FileSystem/CMakeLists.txt`): `add_library(VibeFileSystem STATIC Private/Path.cpp Private/FileSystem.cpp)`
  + alias `Vibe::FileSystem`; `target_include_directories(... PUBLIC Public PRIVATE Private)`;
  `target_link_libraries(VibeFileSystem PUBLIC Vibe::Core Vibe::Platform)`. `Engine/Source/Runtime/CMakeLists.txt`
  ganha `add_subdirectory(FileSystem)`; `Engine/Source/Tests/CMakeLists.txt` ganha `Vibe::FileSystem` no link
  de `VibeTests` (fontes auto-globadas — ADR 0016).

## Plano de testes (lista RED — ordem de execução do `tdd`)
Fixture cria o arquivo temporário com a STL (`std::ofstream`, `std::filesystem::temp_directory_path()`, nome
único, apagado no teardown) e o lê via `FileSystem` (o SUT nunca cria o arquivo — um bug de escrita não mascara um de leitura).

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `FileSystem_Smoke_Task11` | `[filesystem][smoke]` | FileSystem_ReadSync.cpp | escreve temp (11 bytes) → `ReadSync(path)` ok e bytes batem; `ReadSync(path, Dst)` (Dst suficiente) retorna 11 e bytes batem; `Path("a\\b").Str()=="a/b"`; sem leak (asan); < 1 s |
| 2 | `FileSystem_Path_NormalizeSlashes` | `[filesystem][unit]` | FileSystem_Path.cpp | `Path("a\\b\\c").Str()=="a/b/c"`; misto `a/b\\c`→`a/b/c`; já-normalizado inalterado (idempotente) |
| 3 | `FileSystem_Path_Join` | `[filesystem][unit]` | FileSystem_Path.cpp | `Path("dir").Join(Path("f.txt")).Str()=="dir/f.txt"`; base `dir/` não duplica a barra |
| 4 | `FileSystem_Path_Extension` | `[filesystem][unit]` | FileSystem_Path.cpp | `Path("a/b/m.gltf").Extension()==".gltf"`; sem extensão → vazia; `CStr()` nul-terminado bate com `Str()` |
| 5 | `FileSystem_ReadSync_SmallFileMatch` | `[filesystem][unit]` | FileSystem_ReadSync.cpp | conteúdo conhecido → `ReadSync` ok, tamanho e bytes exatos; arquivo de 0 bytes → vetor vazio (não erro) |
| 6 | `FileSystem_OpenMissing_ReturnsErr` | `[filesystem][unit]` | FileSystem_ReadSync.cpp | caminho inexistente (nome único nunca criado) → `!has_value()`, `error()==FileError::NotFound`; sem buffer parcial |
| 7 | `FileSystem_ReadSync_BufferTooSmall` | `[filesystem][unit]` | FileSystem_ReadSync.cpp | `ReadSync(path, Dst)` com `Dst.size()` < arquivo → `FileError::InvalidArgument`; `Dst` exato → contagem == tamanho |

**Smoke**: teste #1, < 30 s. **Determinismo** (HARDENING §7): sem wall-clock/RNG/sleep; conteúdo de bytes
literal; temp único apagado no teardown. **Gate de memória**: `risco_memoria: true` → preset **asan-debug**
verde (buffers de leitura, sem overflow do `Vspan`). TrackingAllocator é N/A (FileSystem não linka Memory —
precedente T07); o gate de memória aqui é asan-debug. **Cobertura**: cada símbolo público dos 3 contratos
aparece em ≥ 1 linha.

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos + gate asan)

```powershell
# Configurar + build (gate: /W4 /WX falham com warning novo)
cmake --preset debug; cmake --preset development
cmake --build --preset debug; cmake --build --preset development
# Testes da task (lista RED)
ctest --preset debug --output-on-failure -R "FileSystem_"
# Smoke da task (com medição de duração)
Build\debug\bin\VibeTests.exe "[filesystem][smoke]" --durations yes
# Suite completa (gate final)
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
# Gate de memória (risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure -R "FileSystem_"
```

## Critério de aceitação
- [ ] 7 TEST_CASE do plano verdes — `ctest --preset debug -R "FileSystem_"`
- [ ] Smoke `FileSystem_Smoke_Task11` < 30 s (alvo < 1 s) — saída de `--durations yes`
- [ ] Suite completa verde em debug e development (não regrediu)
- [ ] **asan-debug verde** — `ctest --preset asan-debug -R "FileSystem_"` (gate de memória; sem overflow do Vspan)
- [ ] `ReadSync(_, Vspan)` recebe `Vspan` (não ptr+size); sem `new`/`malloc` cru; sem C-cast — inspeção
- [ ] Headers Public sem `<windows.h>`/Private/; `FileError` não vaza `PlatformError` — inspeção
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T11/T12), ADR 0020. **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `Path` (ctor, `Join`/`Extension`/`Str`/`CStr`) | T12 (ReadAsync/FileWatcher), T13, T14 |
| `FileError` | T12, T13, T14 |
| `FileSystem::ReadSync` (2 sobrecargas) | T12 (job async reusa), T13, T14 |

## Hardening aplicável (referência + concretização)
- §3/§4/§5 → aqui: `Path`/`FileSystem`/`FileError` PascalCase, `m_Value`; headers em `Public/FileSystem/` (include `<FileSystem/Foo.h>`, ADR 0014); sem `<windows.h>`/Private em Public; Doxygen completo.
- §7 → aqui: a tabela do plano É a aplicação; temp files isolados; determinístico.
- §12 → aqui: `Vspan` na sobrecarga zero-alloc; `std::vector` só em cold path; sem `new` cru; sem C-cast; **gate asan-debug**.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
Formato → orçamento → deps (T07 = Implementado) → pre-flight (`PlatformFile`/`PlatformError` vs Tasks/07;
`Vbyte`/`Vspan`/`VResult` vs Tasks/02) → Em-execução → baseline → branch `task/11-filesystem-sync-path` →
`tdd` sobre a lista RED → gate completo + asan-debug → Implementado → commit `[task 11] filesystem: path + readsync`, push, PR.

## Desvios aprovados
(vazio na criação)

## Bloqueio
(vazio)
## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §9 · Phases/Fase-01-foundation.md · Decisions/0014, 0020
