---
id: 0014
titulo: Convenção de include paths — headers em Public/<Module>/ e forma #include <Module/Foo.h>
data: 2026-06-11
status: Aceita
relacionada: [Decisions/0013, Phases/Fase-01-foundation, Tasks/02-core-types-handles-result]
---

## Contexto

A simulação de executor frio da Task 02 (gate de autossuficiência do formato 2) expôs uma contradição latente: HARDENING §4 prescreve que outros módulos incluem headers via `#include <Module/Foo.h>`, mas o layout planejado colocava os headers diretamente em `Engine/Source/Runtime/<Module>/Public/Foo.h`. Com o include root apontando para `Public/`, a forma `<Module/Foo.h>` não resolve — e a forma plana `<Foo.h>` colidiria entre módulos (a engine terá 25+ módulos; `Types.h`, `Path.h`, `Handle.h` são nomes naturais em vários deles).

## Opções consideradas

- **A**: Headers vivem em `Public/<Module>/Foo.h`; include root de cada alvo = seu próprio `Public/`; forma canônica `#include <Module/Foo.h>`.
- **B**: Include root = `Engine/Source/Runtime/`; forma `#include <Module/Public/Foo.h>`.
- **C**: Include root = `Public/`; forma plana `#include <Foo.h>` (sem prefixo de módulo).

## Decisão

**Opção A.**

```
Engine/Source/Runtime/Core/
    Public/
        Core/              ← subdiretório com o nome do módulo
            Types.h
            Handle.h
    Private/
        StringIdTable.cpp
```

- CMake por módulo: `target_include_directories(Vibe<Module> PUBLIC <dir>/Public PRIVATE <dir>/Private)`.
- Forma canônica de include (inclusive dentro do próprio módulo e nos testes): `#include <Core/Types.h>`.
- A forma `<Module/Foo.h>` torna o módulo de origem visível em todo include, casa literalmente com HARDENING §4, elimina colisões de nome entre módulos, e deixa violações grepáveis (`/Private/` numa linha de include de outro módulo = violação imediata).
- Opção B foi rejeitada por vazar a palavra `Public` em todos os includes; C, por reintroduzir o risco de colisão que o §4 existe para evitar.

## Consequências

- HARDENING §4 ganha a formalização do layout (`Public/<Module>/`) e da forma de include.
- `vx-naming-style` valida: header Public fora de `Public/<Module>/` é VIOLATION; include de `Private/` alheio em qualquer profundidade é VIOLATION.
- Fase-01 e tasks 02/03 atualizam seus `files_create` (ex.: `Core/Public/Core/Types.h`). Árvores ilustrativas em design-mvp.md §6 permanecem como estão (mostram só `Public/`/`Private/`, sem arquivos).
- Custo aceito: um nível extra de diretório (`Core/Public/Core/`) — preço padrão da indústria por hygiene de include em multi-módulo.
