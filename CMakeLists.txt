cmake_minimum_required(VERSION 3.28)

project(VibeEngine LANGUAGES CXX)

# --------------------------------------------------------------------------
# Flags MSVC comuns a todos os presets (ADR 0008).
# As flags/defines POR PRESET vivem em CMakePresets.json; aqui ficam apenas
# as que sao identicas em debug/development/shipping/asan-debug.
# --------------------------------------------------------------------------
if(MSVC)
    add_compile_options(
        /std:c++latest
        /Zc:__cplusplus
        /Zc:preprocessor
        /permissive-
        /W4
        /WX
        /MP
        /utf-8
        /EHsc
    )
endif()

# Defines VIBE_* / TRACY_ENABLE por preset, propagados a todos os alvos
# adicionados abaixo (a partir da T02). Fonte: CMakePresets.json -> VIBE_DEFINES.
if(DEFINED VIBE_DEFINES)
    add_compile_definitions(${VIBE_DEFINES})
endif()

# Executaveis em Build/<preset>/bin/ (contrato T01 para T02 e T14).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Catch2 v3 resolvido via vcpkg (manifest). Nao cria alvo nesta task.
enable_testing()
find_package(Catch2 3 CONFIG REQUIRED)

# --------------------------------------------------------------------------
# Modulos da engine entram a partir da T02 (core-types-handles-result):
#   add_subdirectory(Engine/Source/Runtime/Core)
#   ...
# Nenhum add_subdirectory nesta task (apenas bootstrap de tooling).
# --------------------------------------------------------------------------
