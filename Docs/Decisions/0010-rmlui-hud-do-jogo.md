---
id: 0010
titulo: RmlUi para a HUD do jogo no MVP (editor permanece Dear ImGui)
data: 2026-06-11
status: Aceita
relacionada: [Decisions/0011, Decisions/0013]
---

## Contexto

O design-mvp.md §5.7 previa HUD do jogo via Dear ImGui no MVP, com RmlUi avaliado apenas como migração pós-MVP. O usuário decidiu que a UI do jogo deve ser RmlUi **desde o MVP**: a engine é um case de vibe coding visando jogos action RPG com apresentação realista, e a HUD (HP, stamina, lock-on, prompts, pause menu) é parte da identidade visual do produto — ImGui produz HUD com aparência de ferramenta de debug.

RmlUi estava na lista negativa do HARDENING §2 (herdada do out-of-scope original junto com NoesisGUI). Esta ADR formaliza a saída controlada da lista.

## Opções consideradas

- **A**: Manter ImGui na HUD do MVP; RmlUi pós-MVP (design original §5.7).
- **B**: RmlUi para a HUD do jogo no MVP; editor permanece 100% Dear ImGui + ImGuizmo.
- **C**: RmlUi para jogo E editor.

## Decisão

**Opção B.**

- **Módulo `EngineUI`** (`Engine/Source/Runtime/UI/`, Public/Private) isola o backend. Gameplay consome apenas a interface `EngineUI` — nenhum header RmlUi vaza em `Public/`.
- **Integração RmlUi**:
  - `Rml::RenderInterface` implementado sobre a **RHI** (geometria compilada → vertex/index buffers RHI, scissor, texturas via bindless). Proibido D3D12 direto — mesma regra do §5.5.
  - `Rml::SystemInterface` sobre `Platform` (clock via `HighResClock`, log via `VLOG_*`).
  - Fontes via FontEngine default do RmlUi (FreeType, dependência transitiva do port vcpkg).
  - Submissão de geometria acontece no `UICompositePass` do RenderGraph (§8.8).
- **Assets de UI**: documentos `.rml` + folhas `.rcss` em `Game/Content/UI/`, carregados via `AssetSystem`. Estado dinâmico (HP, stamina, lock-on) via **data bindings** do RmlUi — gameplay escreve num model, nunca manipula elementos.
- **Dependência**: `rmlui` adicionado ao `vcpkg.json` (baseline-locked como as demais).
- **Fase**: a integração RmlUi + HUD + pause menu permanecem na **Fase 12** (§9), onde a HUD já estava. Nenhuma fase anterior é afetada.
- **Editor e debug**: `VibeEditor.exe` permanece Dear ImGui + ImGuizmo (§5.1 intacto). Overlays de debug do runtime (profiler, console em jogo) também permanecem ImGui — RmlUi é exclusivo da UI "de produto" do jogo.
- **NoesisGUI continua banido** (licença comercial, fora do espírito open da stack).

## Consequências

- HARDENING §2: `RmlUi` sai da lista negativa; entra regra explícita de fronteira (RmlUi só em `EngineUI`/HUD; editor fora de ImGui = violação; ImGui em HUD de produto = violação).
- design-mvp.md atualizado: §2 (in-scope), §4 (stack), §5.7 (reescrito), §7 (vcpkg), §8.21 (reescrito), §9 Fase 12, §10 (nota), §12 (novo risco: custo/complexidade do `RenderInterface` sobre RHI — mitigação: usar os backends de exemplo oficiais do RmlUi como referência estrutural).
- `vx-spec-ui-ux` reescrita: HUD = RmlUi (.rml/.rcss, data bindings); editor = ImGui; halts invertidos.
- Caminho de migração antigo ("ImGui sai, RmlUi entra depois") deixa de existir — a evolução pós-MVP acontece **dentro** do RmlUi (temas, animações, telas adicionais).
- Orçamento de frame: o custo de render da HUD entra no orçamento de GPU do §13 do HARDENING (HUD simples do MVP: alvo < 0.5 ms em 1080p).
