# Vibe Engine — MVP

Documento de planejamento para o primeiro entregável funcional da Vibe Engine. O MVP destila o design completo em um produto vertical mínimo que prova a viabilidade técnica da engine e gera um jogo demonstrável em PC Windows. Tudo que não estiver aqui foi conscientemente postergado para após o MVP.

---

# 1. Objetivo

Entregar dois executáveis funcionais em Windows x64:

```txt
VibeEditor.exe
VibeGame.exe
```

Ambos compartilhando o mesmo runtime. O `VibeGame.exe` carrega um nível compacto, controla um personagem em terceira pessoa com gráficos PBR apropriadamente realistas, permite combate corpo a corpo contra um inimigo funcional, com física e áudio integrados. O `VibeEditor.exe` permite abrir o projeto, navegar assets, inspecionar entidades da cena, e iniciar o playmode.

O MVP **não** é um jogo. É a prova de que a arquitetura da Vibe Engine sustenta um loop de gameplay realista de action RPG, com qualidade visual e responsividade compatíveis com referência da indústria, sobre fundação que pode crescer até o design completo sem reescrever o que já existe.

A Vibe Engine é também um **case de vibe coding**: todo o desenvolvimento flui por um pipeline disciplinado de skills (análise de fase → criação de task → execução), governado pelas regras inegociáveis de [Hardening/HARDENING.md](Hardening/HARDENING.md). Dois pilares valem em todas as fases, sem exceção: **performance** (gate de 1080p@60 e teto de design 4K@60 via FSR — ADR 0011) e **gerenciamento de memória exemplar e seguro** (RAII, allocators próprios, zero leaks como gate de task — HARDENING §12).

---

# 2. Escopo

## In scope

```txt
- Plataforma: Windows 10/11 x64
- API gráfica: D3D12 apenas
- Idioma do runtime: C++23
- Tooling: CMake + Ninja + vcpkg
- Editor: Dear ImGui dentro do runtime C++
- Renderer: deferred + Forward+ híbrido com PBR, IBL, CSM, GTAO, SSR, TAA, FSR
- Animation: skeleton, clips, blend tree, state machine, root motion, eventos
- Physics: Jolt com character controller e traces
- Combat: ataques data-driven, hitboxes, dano, stamina, lock-on
- AI: state machine simples para um inimigo com chase/attack/react/die
- Audio: miniaudio com footsteps, impactos e ambience
- Input: teclado/mouse + XInput
- UI do jogo: HUD e pause menu via RmlUi (módulo EngineUI isola o backend — ADR 0010)
- Asset pipeline: loose files com importação on-demand (FBX/glTF/PNG/EXR)
- Editor mínimo: viewport, asset browser, inspector, hierarchy, console, profiler
- Build: profile Debug, Development, Shipping
```

## Out of scope (preserva caminho para o design completo)

```txt
- Backends Vulkan / PS5 / Xbox
- DualSense, haptics, glyphs por plataforma
- Streaming de mundo (WorldPartition, HLOD, impostors)
- Cooked binary pipeline e package builder
- Mesh shaders, work graphs, GPU-driven rendering completo
- Ray tracing, DDGI, path tracing
- DLSS e XeSS (apenas FSR no MVP)
- Editor C# + Avalonia com viewport compartilhada
- Reflection generator baseado em libclang
- Motion matching, pose warping, full-body IK
- Behavior trees, utility AI, squad system
- Quest system, dialogue, inventory completo
- Cinematic sequencer, lipsync, facial animation
- Hot reload de C++
- Networking, multiplayer, cloud saves
- Localization, TRC/XR de plataforma
- Crash reporting integrado (Crashpad)
- Wwise/FMOD, NoesisGUI
- Save versioning robusto e múltiplos slots
- DLC/patch system, virtual file system avançado
```

Tudo nesta lista entra **após** o MVP, em ordem ditada pelas necessidades do jogo-alvo final. A arquitetura do MVP deve deixar os slots prontos sem precisar reabrir os módulos.

---

# 3. Critérios de aceitação

O MVP está pronto quando todos os critérios abaixo são verdadeiros simultaneamente:

```txt
1. VibeEditor.exe abre o projeto, mostra a viewport renderizando o nível
   de teste, e permite navegar a hierarquia da cena e inspecionar
   propriedades de entidades.

2. VibeGame.exe inicia em modo standalone, carrega o nível de teste
   sem o editor, e roda em 1080p a 60 fps estáveis em uma GPU classe
   RTX 3060 / RX 6600.

3. O personagem do player é controlável com gamepad ou teclado/mouse,
   com movimento em terceira pessoa, sprint, dodge, ataque leve, ataque
   pesado e lock-on no inimigo.

4. Um inimigo funcional persegue o player, executa ataques quando em
   range, recebe dano com hit react visível, e morre com ragdoll básico
   ao chegar a 0 HP.

5. Física: o personagem colide com o terreno e geometria estática,
   sobe degraus, e desliza em rampas íngremes. Hitboxes detectam
   acertos em armas.

6. Renderer: o nível tem materiais PBR, sombras dinâmicas com cascade
   shadow maps, iluminação indireta via IBL, ambient occlusion
   por GTAO, anti-aliasing por TAA, e upscaling por FSR.

7. Áudio: passos por superfície, impactos de arma, ambient loop do
   nível, e um cue de música quando o player entra em combate.

8. O editor pode salvar o estado atual da cena em um arquivo e o jogo
   carregar esse mesmo arquivo no executável standalone.

9. Profiler do editor mostra CPU frame time, GPU frame time, e
   contagem de drawcalls em overlay live.

10. O projeto compila do zero em uma máquina nova com Visual Studio
    2022, CMake e vcpkg em até 30 minutos, sem intervenção manual.
```

## 3.1 Meta de design (não-gate): 4K@60 via FSR

Além do gate #2, a arquitetura deve **sustentar 4K @ 60 fps com FSR Quality/Balanced em GPU classe RTX 4070 / RX 7800 XT** sem mudanças estruturais (ADR 0011). Esta meta nunca atrasa a aceitação do MVP — ela apenas veta decisões de arquitetura que a impossibilitem:

- Nenhum passe com custo proporcional à resolução de **saída** além do estritamente necessário (pós-FSR só tonemap/UI).
- Motion vectors, jitter e reactive mask corretos desde a integração TAA/FSR (Fase 4).
- Estruturas per-frame dimensionadas para o pior caso do teto, não apenas para o gate.

`vx-spec-rendering` e `vx-spec-memory-perf` bloqueiam decisões que violem o teto, mesmo que passem no gate.

## 3.2 Pilares de performance e segurança de memória

Orçamento de frame de partida no gate (16.6 ms; refinado por medição Tracy/PIX — ADR 0011, HARDENING §13):

| Fatia | Orçamento inicial |
|---|---|
| CPU game (sim, gameplay, physics 1/60 fixo) | ≤ 4 ms |
| CPU render-prep (visibility, RenderGraph build, submit) | ≤ 2 ms |
| GPU (GBuffer + shadows + lighting + post + UI) | ≤ 10 ms |
| Slack / OS / present | ~0.5 ms |

Pilares de memória (normativos em HARDENING §12):

- RAII em tudo; `new`/`delete`/`malloc` crus proibidos fora de `Memory/` e wrappers RAII.
- Ownership único via `std::unique_ptr` e `VHandle<T>`; raw pointer = observação não-dona.
- `TrackingAllocator` reporta **zero leaks** ao final de todo smoke (Debug/Development).
- Preset `asan-debug` verde obrigatório em toda task com `risco_memoria: true` (ADR 0013).
- Tracy zones em todo sistema per-frame desde a Fase 1 (ADR 0004).

---

# 4. Stack tecnológico

```txt
Linguagem runtime:      C++23
Build system:           CMake 3.28+ com presets, Ninja, vcpkg
Compilador:             MSVC (Visual Studio 2022)
API gráfica:            DirectX 12 (Agility SDK)
Shader compiler:        DXC, HLSL Shader Model 6.6+
Math:                   glm (substituível depois)
Physics:                Jolt Physics
Audio:                  miniaudio
Mesh import:            cgltf, ufbx
Texture import:         DirectXTex, stb_image
Texture compression:    DirectXTex (BC1-BC7)
UI editor + debug:      Dear ImGui (backend D3D12 + Win32)
UI do jogo (HUD):       RmlUi (RenderInterface sobre a RHI, fontes via FreeType)
JSON:                   nlohmann/json
YAML:                   yaml-cpp
Logging:                spdlog
Profiling:              Tracy
Testing:                Catch2
Upscaling:              AMD FSR 2/3 (FidelityFX SDK)
GPU memory:             D3D12MA
GPU debug:              PIX for Windows, RenderDoc
```

---

# 5. Decisões pragmáticas adotadas para o MVP

Decisões que divergem do design completo, com justificativa explícita. Cada uma tem um caminho de migração planejado para depois do MVP.

## 5.1 Editor em Dear ImGui, não em C# + Avalonia

O design completo prevê editor C# + Avalonia com viewport compartilhada via C API estável e texture sharing. Isso é a arquitetura certa de longo prazo, mas exige meses de tooling antes de produzir o primeiro pixel. Para o MVP, Dear ImGui rodando dentro do runtime C++ entrega 90% da funcionalidade de editor (viewport, asset browser, inspector, hierarchy, console, profiler) em fração do tempo, com o backend D3D12 já existente.

**Migração planejada**: a separação `EngineCore` vs `EngineEditor` será mantida desde o MVP em módulos distintos. O editor Avalonia consumirá os mesmos serviços de editor (asset browser, inspector schema, scene API) que o ImGui consome, apenas com frontend diferente.

## 5.2 Sem reflection generator próprio

O design completo descreve um reflection generator com macros `VCLASS()` e `VPROPERTY()`. Construir isso com libclang é projeto de meses. Para o MVP, registro manual de propriedades via templates e Boost.PFR para structs simples cobre o que o inspector precisa.

**Migração planejada**: o sistema de inspector consome um `TypeRegistry` abstrato. No MVP esse registry é populado manualmente. Pós-MVP, o reflection generator alimenta o mesmo registry sem mudança no inspector.

## 5.3 Sem cooker e sem pacotes

O design completo prevê assets cooked em binário (`.vmesh`, `.vtex`, `.vmat`) empacotados em `.vpkg`. Para um nível pequeno em PC, loose files com importação on-demand é suficiente e drasticamente mais rápido para iterar.

**Migração planejada**: o `AssetSystem` carrega via `AssetHandle` opaco. No MVP, o handle resolve para loose file. Pós-MVP, resolve para entrada em package. Código cliente não muda.

## 5.4 Job system com workers, não fibers

O design lista `FiberJobSystem` como opção. Fibers têm custos de complexidade, depuração e portabilidade que não compensam no MVP. Worker thread pool com task graph em estilo enkiTS/TaskFlow entrega o necessário.

**Migração planejada**: a API de jobs (`JobSystem::Schedule`, `JobFence`, `ParallelFor`) é a mesma. Se fibers se tornarem necessárias em fase tardia, o backend troca sob a mesma API.

## 5.5 RHI projetada multi-backend, mas só D3D12 implementado

A interface `RHIDevice`, `RHICommandList`, `RHITexture`, etc. existe desde o início, com semântica próxima de D3D12/Vulkan (barriers explícitas, descriptor heaps, bindless). Mas apenas o backend D3D12 é implementado no MVP.

**Migração planejada**: backend Vulkan vem depois sob a mesma RHI. Backends de console vêm com acesso ao SDK respectivo, sem afetar gameplay.

## 5.6 Shader em HLSL puro, sem Slang

Slang é promissor mas o ecossistema é jovem. HLSL + DXC produzindo DXIL é o caminho seguro. Quando Vulkan for adicionado, DXC já produz SPIR-V do mesmo HLSL.

**Migração planejada**: avaliar Slang quando o número de shaders justificar suas features de módulos e generics, provavelmente bem além do MVP.

## 5.7 UI do jogo via RmlUi desde o MVP (ADR 0010)

O design original avaliava RmlUi e NoesisGUI apenas para depois do MVP, com HUD provisória em Dear ImGui. Decisão revista (ADR 0010): a UI do jogo (HP, stamina, lock-on, prompts, pause menu) é **RmlUi já no MVP**, através do módulo `EngineUI` que isola o backend — gameplay nunca vê headers RmlUi, só escreve em data models. O editor e os overlays de debug do runtime permanecem Dear ImGui (§5.1). NoesisGUI permanece fora (licença comercial).

**Migração planejada**: nenhuma troca de backend prevista — a evolução pós-MVP acontece dentro do RmlUi (temas, animações, telas adicionais). O `RenderInterface` é implementado sobre a RHI, então backends gráficos futuros (Vulkan) não afetam a UI.

## 5.8 Audio com miniaudio, sem Wwise/FMOD

Wwise e FMOD são excelentes mas têm modelos de licença que escalam com receita ou exigem upfront. Para MVP, miniaudio é open source, single-header, suficiente.

**Migração planejada**: módulo `EngineAudio` com interface própria. Substituir backend não afeta a API do gameplay.

## 5.9 Sem streaming, nível inteiro carregado

Streaming exige world partition, scheduler, async upload de GPU resources, HLOD. Tudo isso vem depois. O nível do MVP cabe em memória de uma vez.

**Migração planejada**: o sistema de cena já carrega via `LoadLevelAsync` mesmo que o backend atual leia tudo de uma vez. Pós-MVP, o backend ganha streaming chunk-based.

## 5.10 Sem versioning de save robusto

Save único, formato binário com header de versão simples. Sem migração entre versões. Sobrescreve no slot único.

**Migração planejada**: trocar o serializador binário por um com schema versionado quando o jogo tiver conteúdo a preservar.

## 5.11 Renomear `WorldPartition`

Nome usado pela Unreal Engine como trademark. Adotar `WorldStream` ou `StreamingPartition`. Decisão pequena, evita confusão.

---

# 6. Estrutura de pastas

```txt
vibe-engine/
    CMakePresets.json
    CMakeLists.txt
    vcpkg.json
    README.md

    Engine/
        Source/
            Runtime/
                Core/
                    Public/
                    Private/
                Platform/
                    Public/
                    Private/
                        Windows/
                Math/
                Memory/
                JobSystem/
                FileSystem/
                EngineCore/
                    Public/
                    Private/

                RHI/
                    Public/
                    Private/
                        D3D12/

                Renderer/
                    Public/
                    Private/
                        RenderGraph/
                        Passes/
                        Lighting/
                        Shadows/
                        PostProcess/

                ShaderSystem/
                MaterialSystem/
                TextureSystem/
                MeshSystem/

                AssetSystem/
                World/
                ECS/
                Animation/
                Character/
                Physics/
                AI/
                Gameplay/
                Combat/
                Audio/
                Input/
                Camera/
                UI/
                SaveGame/

            Editor/
                EditorApp/
                Viewport/
                AssetBrowser/
                Inspector/
                Hierarchy/
                Console/
                Profiler/

            Tools/
                AssetImporter/

        Shaders/
            Common/
            Mesh/
            Materials/
            Lighting/
            Shadows/
            PostProcess/
            FullScreen/

        Content/
            Engine/
                Materials/
                Textures/
                Meshes/

    Game/
        Source/
            GameRuntime/
        Content/
            Characters/
            Environments/
            Animations/
            Materials/
            UI/
            Audio/
            Combat/
        Config/

    ThirdParty/
        README.md

    Saved/
    Intermediate/
    Build/
```

Cada módulo tem separação rígida entre `Public/` (headers expostos) e `Private/` (implementação interna). Isso evita o "bolo de includes" descrito no design original.

`EngineCore/` orquestra Initialize/Shutdown de todos os módulos da fundação (ADR 0006, ADR 0012). Módulo novo não listado aqui exige ADR + emenda desta seção no mesmo PR (ADR 0012).

---

# 7. Dependências externas (via vcpkg)

```txt
directx-headers
directxtex
directxshadercompiler
d3d12-memory-allocator
fidelityfx-sdk
joltphysics
miniaudio
cgltf
ufbx
stb
glm
nlohmann-json
yaml-cpp
spdlog
tracy
catch2
imgui (com backends win32 + dx12)
rmlui
boost-pfr
```

Versões fixas em `vcpkg.json` com baseline lockado para reprodutibilidade.

---

# 8. Arquitetura mínima dos módulos

## 8.1 Core

```txt
Types         (int sizes [Vint8, Vuint8, Vint16, Vuint16, Vint32, Vuint32, Vint64, Vuint64], fixed-width [Vfloat, Vdouble], byte [Vbyte], span [Vspan], string [Vstring], optional)
Assert        (VASSERT, VVERIFY, VCHECK com break em debug)
Logging       (VLOG_INFO, VLOG_WARN, VLOG_ERROR via spdlog)
Time          (FrameTimer, HighResClock)
StringId      (hash de string para nomes/tags) [VStringId("Player"), VStringId("Enemy")]
Handle<T>     (handle tipado com generation counter) [VHandle<Mesh>, VHandle<Texture>]
Result<T,E>   (success/error sem exceções) [VResult<Mesh, LoadError>]
```

Sem dependência externa além de C++ standard e spdlog.

## 8.2 Platform/Windows

```txt
WindowsApplication    (main loop, message pump)
Window                (HWND wrapper, swapchain target)
DynamicLibrary        (LoadLibrary wrapper)
File                  (Win32 file I/O)
Thread                (std::thread wrapper + affinity)
PerformanceCounter    (QueryPerformanceCounter)
```

## 8.3 Math

`glm` como baseline. Tipos próprios `Vec2/3/4`, `Mat4`, `Quat`, `Transform` como typedefs/wrappers finos com `V` prefixo. Substituição por math própria fica para pós-MVP se necessário.

## 8.4 Memory

```txt
LinearAllocator
StackAllocator
PoolAllocator
FrameAllocator        (limpa por frame)
TrackingAllocator     (debug)
MemoryTag             (categoria por subsistema)
```

D3D12MA cuida da memória de GPU. CPU usa o sistema acima mais o heap padrão para casos comuns.

## 8.5 JobSystem

```txt
WorkerThread
JobQueue
TaskGraph
JobHandle
ParallelFor
```

N workers = núcleos físicos - 1. API de submit é não-bloqueante. `Wait()` em `JobHandle` para sincronizar.

## 8.6 RHI (D3D12)

```txt
RHIDevice
RHIAdapter
RHICommandQueue
RHICommandList
RHISwapchain
RHIBuffer
RHITexture
RHISampler
RHIShader
RHIPipelineState
RHIRootSignature
RHIDescriptorHeap
RHIFence
RHIBindlessTable
```

Triple buffering, frame allocator de upload heap, bindless descriptor heap único para textures/buffers. Barriers explícitas resolvidas pelo RenderGraph.

## 8.7 RenderGraph

```txt
RenderGraph
RenderPass
ResourceHandle
TransientTexture
TransientBuffer
BarrierSolver
```

Cada frame, o renderer monta um grafo declarativo de passes. O graph resolve dependências, aloca recursos transientes, insere barriers, e executa. Sem isso, D3D12 vira inviável quando o pipeline cresce.

## 8.8 Renderer

```txt
RenderWorld           (cópia thread-safe de dados da cena)
RenderScene
RenderView            (camera, frustum)
VisibilitySystem      (frustum culling CPU no MVP)
GBufferPass
DirectionalShadowPass (CSM com 4 cascades)
LightingPass          (deferred + Forward+ para transparentes)
GTAOPass
SSRPass               (screen-space reflections básico)
SkyAtmospherePass     (sky procedural simples)
TAAPass
FSRPass               (FSR2/3 upscale)
TonemapPass
UICompositePass
```

PBR Metallic/Roughness baseline. Cascade shadow maps 4-tap. GTAO em qualidade média. SSR como complemento ao IBL. FSR em modo Balanced no MVP, com motion vectors corretos, reactive mask e jitter no projection.

## 8.9 ShaderSystem

```txt
ShaderCompiler        (DXC wrapper)
ShaderCache           (DXIL cache em disco)
ShaderPermutation
HotReload             (file watcher recompila DXIL ao salvar HLSL)
```

Hot reload de shader é obrigatório no MVP. Salva tempo brutal em iteração visual.

## 8.10 MaterialSystem

```txt
MaterialTemplate      (define inputs e permutações)
MaterialInstance      (valores concretos: textures, escalares)
MaterialParameterBlock (constant buffer leve por instance)
```

Templates do MVP:
- `PBR_Opaque`
- `PBR_Masked`
- `PBR_Skin`
- `PBR_Hair_Simple`
- `Decal`
- `Unlit`

## 8.11 AssetSystem

```txt
AssetHandle<T>        (handle tipado, opaco)
AssetRegistry         (path → handle, metadata)
AssetLoader<T>        (interface de loading)
AsyncLoadQueue
HotReload             (file watcher)
```

No MVP, `AssetLoader<Mesh>` lê glTF/FBX direto via cgltf/ufbx no worker thread, gera vertex/index buffers, faz upload via RHI. `AssetLoader<Texture>` faz pipeline similar com DirectXTex. Sem cooker.

## 8.12 World + ECS

```txt
World
EntityRegistry        (sparse set, entity id)
ComponentStorage<T>
Archetype             (chunk de entidades com mesmo conjunto de components)
System
```

Híbrido: ECS para dados massivos (transforms, render data, animation state), Actor/Component para gameplay autoral (player, enemy, NPCs).

Components do MVP:
```txt
TransformComponent
StaticMeshComponent
SkeletalMeshComponent
LightComponent
CameraComponent
RigidBodyComponent
CharacterControllerComponent
AnimationComponent
CombatantComponent
HealthComponent
StaminaComponent
AIComponent
AudioEmitterComponent
PlayerInputComponent
```

## 8.13 Animation

```txt
Skeleton
AnimationClip
AnimationPose
AnimationGraph        (blend tree + state machine)
BlendNode
StateMachineNode
RootMotion
AnimationNotify       (eventos discretos)
IKChain               (apenas two-bone IK no MVP, para mira/posição da mão na arma)
```

State machine do player no MVP:
```txt
Locomotion (BlendSpace velocity)
    → Dodge
    → Attack_Light
    → Attack_Heavy
    → HitReact
    → Dead
```

Notifies do MVP:
```txt
AttackWindowStart / AttackWindowEnd
ComboWindowStart / ComboWindowEnd
DamageFrame
Footstep
WeaponTrailStart / WeaponTrailEnd
```

## 8.14 Physics (Jolt)

```txt
PhysicsWorld
RigidBody
Collider              (Box, Sphere, Capsule, ConvexHull, MeshShape)
CharacterController   (Jolt's CharacterVirtual)
QuerySystem           (raycast, sweep, overlap)
RagdollProfile        (definição em asset, ativação on death)
PhysicsLayer          (PlayerHitbox, EnemyHitbox, World, Player, Enemy)
```

Jolt roda em job pool dedicado. Step de 1/60s fixo com interpolação para render.

## 8.15 Character

```txt
CharacterIntent       (input desejado: direção, sprint, dodge, attack)
CharacterMotor        (resolve intent + animation + physics)
RootMotionController
LockOnComponent
HitReactionComponent
```

## 8.16 Combat

```txt
CombatantComponent
HitboxSystem          (volumes que aparecem durante DamageFrame)
HurtboxSystem         (volumes do alvo, mapeados por bone)
AttackDefinition      (asset data-driven)
ComboGraph            (transições entre attacks)
DamageSystem
StatusEffect          (apenas Stagger e Stunned no MVP)
LockOnSystem
```

Asset de ataque (formato JSON no MVP):
```json
{
  "id": "player_light_attack_01",
  "animation": "Player_Attack_Light_01",
  "staminaCost": 12,
  "damage": 28,
  "poiseDamage": 15,
  "hitFrames": [
    { "start": 0.24, "end": 0.38, "socket": "weapon_blade" }
  ],
  "comboWindow": {
    "start": 0.42,
    "end": 0.68,
    "next": "player_light_attack_02"
  }
}
```

## 8.17 AI

State machine simples para o inimigo único do MVP:

```txt
Idle
    → Patrol (se sem player na perception)
    → Chase  (se player avistado, fora de attack range)
    → Attack (se player em attack range)
    → HitReact
    → Dead
```

Sem behavior tree, sem utility, sem squad. Apenas o necessário para validar combat loop. Perception é cone de visão simples + raycast LOS.

## 8.18 Camera

```txt
CameraRig
ThirdPersonCamera     (spring arm com collision pull-in)
LockOnCamera          (framing entre player e target)
CameraBlend
CameraShake
```

Dois modos no MVP: `Exploration` e `LockOn`. Transição com blend de 0.2s.

## 8.19 Input

```txt
InputAction
InputContext
InputDevice           (Keyboard, Mouse, XInputGamepad)
ActionMapping         (JSON)
```

Contexts do MVP: `Gameplay`, `UI`, `Debug`.

## 8.20 Audio

```txt
AudioDevice           (miniaudio backend)
SoundAsset
AudioEmitterComponent (spatial 3D)
AudioBus              (Master, SFX, Music, UI)
ReverbZone            (apenas um zone type no MVP)
MusicLayer            (Exploration + Combat com crossfade)
```

## 8.21 UI (EngineUI + RmlUi)

```txt
EngineUI              (interface pública; isola o backend RmlUi)
UIContext             (wrapper de Rml::Context; um por viewport do jogo)
UIDocument            (documento .rml carregado via AssetSystem)
UIDataModel           (data bindings: HP, stamina, lock-on, prompts)
UIRenderInterface     (Rml::RenderInterface sobre a RHI — sem D3D12 direto)
UISystemInterface     (clock via HighResClock, log via VLOG_*)
```

HUD do MVP em RmlUi (ADR 0010): barra de HP, barra de stamina, indicador de lock-on, prompts de interação, pause menu. Documentos `.rml` + folhas `.rcss` em `Game/Content/UI/`, carregados via `AssetSystem`. Estado dinâmico via data bindings — gameplay escreve no model, nunca manipula elementos. Geometria submetida no `UICompositePass` do RenderGraph (§8.8). Custo-alvo da HUD do MVP: < 0.5 ms de GPU em 1080p (HARDENING §13).

Overlays de debug do runtime (console em jogo, profiler overlay) permanecem Dear ImGui — RmlUi é exclusivo da UI de produto do jogo.

## 8.22 SaveGame

```txt
SaveSlot
SaveSerializer        (binário, header com versão)
```

Salva: posição do player, HP/stamina, estado do inimigo (vivo/morto). Slot único. Sobrescreve.

## 8.23 Editor

Editor é um executável separado que linka tudo do runtime mais módulos de editor.

```txt
EditorApp             (main, layout ImGui)
Viewport              (renderiza a cena em uma RTV exibida via ImGui::Image)
AssetBrowser          (árvore de Content/, drag-and-drop básico)
Inspector             (lê TypeRegistry e desenha campos)
Hierarchy             (árvore de entidades da cena)
Console               (saída de logs, comandos)
Profiler              (overlay com Tracy zones, frame timing, drawcalls)
PlayMode              (botão Play → instancia World e roda como o game)
```

---

# 9. Plano de implementação por fases

Cada fase deve terminar com um build verificável e demonstrável. Não passar para a próxima fase sem fechar a anterior.

## Fase 1 — Foundation

```txt
- Setup CMake + Ninja + vcpkg + presets
- Core: Types, Assert, Logging, Time, Handle, Result
- Platform/Windows: WindowsApplication, Window, File, Thread
- Math: wrappers sobre glm
- Memory: LinearAllocator, FrameAllocator, MemoryTag
- JobSystem: worker pool, TaskGraph básico, ParallelFor
- FileSystem: read sync, read async, file watcher
- Catch2 com 20+ testes cobrindo Core/Math/Memory/Job
```

Critério: executável de teste roda 8 workers, lê arquivo async, loga via spdlog, todos os testes passam.

## Fase 2 — RHI D3D12

```txt
- Device, Adapter (DXGI), feature level check
- Command queue (graphics + copy + compute), command lists
- Swapchain triple buffered, present
- Fence/event sync
- Buffers (upload, default, readback) via D3D12MA
- Textures (2D, cubemap) com mip chain
- Sampler library
- Bindless descriptor heap único (CBV/SRV/UAV)
- PSO + RootSignature
- Shader compile via DXC, DXIL cache em disco
- Renderiza triângulo, depois quad texturizado, em fullscreen
```

Critério: tela limpa em uma cor, depois um triângulo, depois um quad com textura PNG carregada, com hot reload de HLSL funcionando.

## Fase 3 — Render Graph e renderer básico

```txt
- RenderGraph com transient resources e barrier solver
- Passes: ClearRT, ForwardOpaque (apenas para protótipo), ImGui
- Integração Dear ImGui (backend D3D12 + Win32)
- Tracy + frame timing
```

Critério: cena com cubo PBR placeholder iluminado por uma directional light, com overlay ImGui mostrando frame timing.

## Fase 4 — Renderer realista

```txt
- GBuffer (Albedo, Normal, MetallicRoughness, Motion)
- Deferred lighting (sun + dynamic point/spot)
- CSM 4 cascades para sun
- IBL: prefilter cubemap + BRDF LUT pré-computados
- GTAO compute pass
- SSR básico
- Sky atmosphere procedural simples
- TAA
- FSR2/3 integrado com motion vectors
- Tonemap (ACES)
```

Critério: cena com sphere material PBR sob HDR sky, com sombras nítidas, AO correto, reflexos, e FSR rodando em modo Balanced sem artifacts grosseiros.

## Fase 5 — Asset system e import

```txt
- AssetHandle + AssetRegistry
- AssetLoader<Mesh> via cgltf + ufbx
- AssetLoader<Texture> via DirectXTex (carrega .dds e converte PNG/EXR para BC7 em runtime se necessário)
- AssetLoader<Material> via JSON
- File watcher trigger hot reload
- Tool de linha de comando AssetImporter para pré-converter offline (opcional no MVP)
```

Critério: importa um modelo glTF do Sketchfab/Mixamo com texturas, vê renderizado na cena corretamente, editar o .json do material e ver atualizar live.

## Fase 6 — World, ECS, Scene

```txt
- EntityRegistry, ComponentStorage
- Componentes: Transform, StaticMesh, Light, Camera
- Scene serialization (JSON no MVP)
- Frustum culling CPU
- GPU scene buffer (transforms + instance data) atualizado por frame
```

Critério: nível com 50+ static meshes, várias lights, câmera navegável (debug fly cam), serialização round-trip funcionando.

## Fase 7 — Animation e Character

```txt
- Skeleton, AnimationClip (import glTF)
- AnimationPose, blend simples
- StateMachine + BlendSpace para Locomotion
- RootMotion
- AnimationNotify
- CharacterMotor stub (sem física ainda, movimento direto)
- ThirdPersonCamera com spring arm
```

Critério: personagem importado do Mixamo se move pelo nível em terceira pessoa, com locomotion blend (idle/walk/run), câmera segue corretamente.

## Fase 8 — Physics (Jolt)

```txt
- Jolt integrado em PhysicsWorld
- PhysicsLayer, mask matrix
- Static mesh collision (TriangleMeshShape gerada do MeshAsset)
- CharacterVirtual integrado ao CharacterMotor
- Raycast, sweep, overlap queries expostos
- Ragdoll profile (def via asset, ativação on death)
```

Critério: personagem colide com geometria do nível, sobe degraus, desliza em rampas íngremes, ragdoll ativa ao morrer.

## Fase 9 — Combat e Inimigo

```txt
- CombatantComponent, Health, Stamina
- HitboxSystem + HurtboxSystem com sockets em bones
- AttackDefinition asset (JSON)
- ComboGraph com janelas de combo
- LockOnSystem
- DamageSystem
- HitReactionComponent
- AI state machine simples para o inimigo
- Spawning do inimigo no nível
```

Critério: player executa light attack, heavy attack, combo de 3 light, dodge cancela ataque, lock-on no inimigo. Inimigo persegue, ataca em range, recebe dano com hit react, morre com ragdoll.

## Fase 10 — Audio

```txt
- AudioDevice via miniaudio
- AudioEmitterComponent espacial
- SoundAsset (.wav, .ogg)
- Buses: Master, SFX, Music, UI
- Footsteps por surface (lookup via material no chão)
- Animation notify dispara áudio (passos, swing, impact)
- MusicLayer com Exploration/Combat e crossfade
```

Critério: passos audíveis com variação por superfície, swing de arma soa, impacto soa, música muda quando entra em combate (player com target lock e enemy aware).

## Fase 11 — Editor

```txt
- EditorApp como executável separado
- Layout ImGui dockable: Viewport, Hierarchy, Inspector, AssetBrowser, Console, Profiler
- Viewport renderiza a cena em RTV exibida via ImGui::Image
- Gizmo de manipulação (translate/rotate/scale) via ImGuizmo
- Inspector com TypeRegistry manual para os components do MVP
- AssetBrowser com thumbnail mínimo (ícone por tipo)
- PlayMode: botão Play instancia o World e roda como standalone
- Save/Load de scene a partir do editor
```

Critério: editor abre, navega a hierarquia, edita position de um light, salva, abre o game standalone, light aparece movido.

## Fase 12 — Game executável e polish

```txt
- VibeGame.exe separado, sem editor linkado
- Entry point que carrega scene Main.scene
- EngineUI + backend RmlUi: UIRenderInterface sobre a RHI, UISystemInterface,
  carregamento de .rml/.rcss via AssetSystem (ADR 0010)
- HUD via RmlUi (HP, stamina, lock-on indicator) com data bindings
- Pause menu via RmlUi, restart, quit
- SaveGame slot único
- Profiles: Debug, Development, Shipping
- Empacotamento do build Shipping em pasta com binários, shaders, content
- README com instruções de build do zero
```

Critério: o game roda standalone, dá pra jogar de ponta a ponta o loop combat, salvar, sair, abrir, continuar — com HUD e pause menu renderizados via RmlUi.

---

# 10. O que NÃO faz parte do MVP e por quê

Esta seção é tão importante quanto a anterior. Sem disciplina aqui, o MVP nunca fecha.

```txt
- Backend Vulkan
    Tempo de implementação não justifica antes do D3D12 estar
    sólido. Adicionar depois sob a mesma RHI.

- Backends de console (PS5, Xbox)
    Exigem acesso a SDK proprietário, devkit, NDA. Sem viabilidade
    de design real antes do acesso.

- Streaming de mundo
    Nível pequeno cabe em RAM. WorldPartition, HLOD, impostors,
    DirectStorage entram quando o jogo tiver mundo grande.

- Cooked binary pipeline
    Loose files são adequados para iteração em PC durante o MVP.
    Cooker entra antes do shipping comercial, não antes do MVP.

- Mesh shaders e GPU-driven completo
    Path tradicional vertex/index é suficiente para um nível pequeno.
    GPU-driven culling, indirect draws, meshlets, são otimizações
    para mundo grande.

- Ray tracing, DDGI, path tracing
    IBL + CSM + SSR + GTAO entrega qualidade visual apropriadamente
    realista. RT é luxo para depois.

- DLSS e XeSS
    FSR cobre o necessário para o MVP e é totalmente open source.
    DLSS exige NVIDIA SDK e fluxo de licença. XeSS é menos crítico.

- Editor C# + Avalonia
    Dear ImGui entrega editor funcional em fração do tempo.
    Avalonia entra quando o editor precisar de UX profissional para
    designers/artistas, não para o programador solo do MVP.

- Reflection generator com libclang
    Registro manual cobre os ~15 component types do MVP. Generator
    é trabalho de meses melhor gasto em renderer/character.

- Behavior trees, utility AI
    Um inimigo com state machine simples valida o combat loop.
    AI sofisticada entra quando houver variedade de inimigos.

- Motion matching, full-body IK, pose warping
    Blend tree + state machine + root motion entrega animação
    presentável. Motion matching é projeto próprio de meses.

- Cinematic, dialogue, lipsync, facial animation
    Não fazem parte do loop core (mover/lutar). Entram depois.

- Quest, dialogue, inventory completo
    MVP é um arena com um inimigo. Sistemas de RPG narrativo
    vêm quando houver narrativa para sustentar.

- Networking, multiplayer, cloud saves
    Out of scope absoluto. Engine offline first.

- Localization, TRC/XR
    Não relevante até abordar shipping ou consoles.

- Hot reload de C++
    Hot reload de shader e de asset cobre 90% da iteração.
    Hot reload de C++ é otimização tardia.

- Crash reporting integrado
    Minidump nativo do Windows é suficiente para MVP.
    Crashpad/Sentry depois.

- Save versioning robusto
    Single slot, single version no MVP. Versioning quando
    o conteúdo persistido importar.

- Plugin system
    Engine monolítica é mais simples para solo dev no MVP.
    Plugins entram quando houver razão concreta para isolar
    sistemas.
```

---

# 11. Critérios de "pronto" por sistema

Tabela curta para auditar progresso. Cada sistema é "pronto para MVP" quando todos os checks são verdadeiros.

```txt
Core
    [ ] Logging, Assert, Time, Handle funcionam
    [ ] Testes passam em CI local

Platform Windows
    [ ] Window criável, message pump rodando
    [ ] FileSystem com I/O async e file watcher

Job System
    [ ] N workers escalonados por núcleo físico
    [ ] ParallelFor escala em throughput
    [ ] TaskGraph com fences resolve dependências

RHI D3D12
    [ ] Triple buffer swapchain estável
    [ ] Bindless heap único funcionando
    [ ] Hot reload de shader < 1s

Render Graph
    [ ] Barriers automáticas corretas (validate via PIX)
    [ ] Transient memory reaproveitada entre passes

Renderer
    [ ] GBuffer, Deferred, CSM, IBL, GTAO, SSR, TAA, FSR
    [ ] Tonemap ACES com exposure controlável
    [ ] 60 fps em 1080p numa RTX 3060 com cena do MVP

Asset System
    [ ] Import de glTF e FBX
    [ ] Import de PNG/EXR para BC7
    [ ] Hot reload de material e textura

World/ECS
    [ ] Scene serializa e desserializa round-trip
    [ ] 50+ entidades estáveis a 60fps

Animation
    [ ] Locomotion blend, attack montages
    [ ] Root motion sincronizado com physics
    [ ] Notifies disparam corretamente

Physics
    [ ] CharacterVirtual em geometria estática
    [ ] Raycast/sweep expostos ao gameplay
    [ ] Ragdoll on death

Combat
    [ ] Player combo de 3 light + heavy + dodge
    [ ] Lock-on com câmera framing
    [ ] Hitbox/hurtbox detecta acertos corretos

AI
    [ ] Inimigo persegue, ataca, react, morre
    [ ] Perception com cone + LOS

Audio
    [ ] Footsteps por surface
    [ ] Impactos via notify
    [ ] Música crossfade explore/combat

Camera
    [ ] ThirdPersonCamera com collision pull-in
    [ ] LockOnCamera com framing player+target

Input
    [ ] Keyboard/Mouse e XInput
    [ ] Action mapping via JSON

UI
    [ ] HUD com HP, stamina, lock-on
    [ ] Pause menu funcional

Save
    [ ] Save/Load do estado do MVP

Editor
    [ ] Viewport renderiza a cena
    [ ] Hierarchy + Inspector editáveis
    [ ] PlayMode roda como game standalone
```

---

# 12. Riscos e mitigações

```txt
Risco: render graph mal projetado vira gargalo de arquitetura.
Mitigação: gastar tempo de design extra na Fase 3, estudar
           render graphs de Frostbite/UE5/Granite antes de
           implementar. Cobrir com testes que validem barriers
           via PIX.

Risco: animação parece "boneca" sem motion matching.
Mitigação: investir em transições de blend tree bem ajustadas,
           foot IK simples, e curvas de aceleração caprichadas
           no character motor. Para MVP, animação "boa" não
           precisa ser "AAA polida".

Risco: Jolt CharacterVirtual com bugs em degraus/slopes.
Mitigação: estudar samples oficiais do Jolt, copiar setup deles
           inicialmente, ajustar parâmetros depois. Não tentar
           character controller custom no MVP.

Risco: combat loop sem game feel (impactos fracos, hits sem peso).
Mitigação: investir em hitstop, screen shake, knockback, audio
           de impact, particle hit, animation hold no contato.
           É o que separa combate funcional de combate satisfatório.

Risco: editor Dear ImGui sente que limita iteração.
Mitigação: aceitar a limitação. Editor é ferramenta interna do
           MVP. Polimento de UX entra com a migração para Avalonia.

Risco: shader hot reload causa crashes/state corrupto.
Mitigação: invalidar PSO cache na recompilação, recriar pipelines
           sob demanda. Catch fail no compile e manter shader
           antigo se o novo falhar.

Risco: assets sem cooker fazem startup lento.
Mitigação: tools/AssetImporter pré-converte texturas para BC7
           offline, salvas como .dds. Loose files .dds + .gltf
           carregam rápido o suficiente para um nível pequeno.

Risco: escopo cresce durante o desenvolvimento.
Mitigação: este documento é o contrato. Qualquer feature que
           não esteja explícita aqui vai para um arquivo
           "PostMVP_Backlog.md". Sem exceção até o MVP fechar.

Risco: motivação cai em fases longas (Renderer, Animation).
Mitigação: cada fase deve gerar algo visualmente demonstrável.
           "First triangle", "first textured cube", "first PBR
           sphere", "first character walking" são marcos
           psicológicos que sustentam ritmo.

Risco: RenderInterface do RmlUi sobre a RHI custa mais que o previsto.
Mitigação: usar os backends de exemplo oficiais do RmlUi como
           referência estrutural; UICompositePass dedicado no
           RenderGraph; HUD do MVP é simples (alvo < 0.5 ms de GPU).
           Fallback NÃO é voltar para ImGui — é reduzir o escopo
           visual da HUD (ADR 0010).

Risco: regressão silenciosa de memória (leak, use-after-free).
Mitigação: gates do HARDENING §12 — TrackingAllocator zero-leak em
           todo smoke, preset asan-debug obrigatório em tasks com
           risco_memoria, VHandle com generation counter detectando
           use-after-free na fundação.
```

---

# 13. Recomendação de ordem mental do esforço

Não tente fazer o renderer perfeito antes de ter um personagem se mexendo. Não tente fazer o character motor perfeito antes de ter combat básico. A ordem das fases acima é deliberada: cada uma deveria custar 1-3 semanas de trabalho focado. O risco maior em projeto solo é gastar 6 meses no Renderer e nunca chegar a Combat. O contrato do MVP é jogar de ponta a ponta uma luta apertada com um inimigo, e depois melhorar tudo.

Quando o MVP fechar, a Vibe Engine terá:

```txt
- Fundação multiplataforma já estruturada (RHI, modules)
- Renderer realista pronto para evoluir (DDGI, RT, mesh shaders)
- Asset system pronto para receber cooker
- Animation pronta para receber motion matching
- Combat pronto para receber abilities, status effects, perks
- Editor pronto para receber Avalonia frontend
- Save pronto para receber versioning
- Audio pronto para receber Wwise/FMOD
```

O MVP é, portanto, a etapa em que a arquitetura do design completo deixa de ser hipótese e passa a ser código que funciona. Tudo depois disso é evolução incremental sobre algo provado.
