cmake_minimum_required(VERSION 3.28)

project(VibeEngine LANGUAGES CXX)

# --------------------------------------------------------------------------
# ADR 0008: as flags por preset (CMAKE_CXX_FLAGS / linker flags, em CMakePresets.json)
# sao a UNICA fonte de verdade de otimizacao e runtime. A toolchain forca
# CMAKE_BUILD_TYPE=Debug, o que faria o MSVC injetar /Od /Ob0 /RTC1 /MDd em TODOS os
# presets (mascarado em debug, mas sobrescreve /O2 /MD em development/shipping -> D9025).
# Neutralizamos os defaults por configuracao e a selecao automatica de runtime.
foreach(VibeCfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_CXX_FLAGS_${VibeCfg} "" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_${VibeCfg} "" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_${VibeCfg} "" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_${VibeCfg} "" CACHE STRING "" FORCE)
endforeach()
set(CMAKE_MSVC_RUNTIME_LIBRARY "")

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
# Modulos da engine (T02+) e alvo de testes.
# --------------------------------------------------------------------------
add_subdirectory(Engine/Source/Runtime)
add_subdirectory(Engine/Source/Tests)
