# Módulo Core — tipos fundamentais da engine (ADR 0001/0009/0014).
add_library(VibeCore STATIC
    Private/StringIdTable.cpp
    Private/AssertImpl.cpp
)
add_library(Vibe::Core ALIAS VibeCore)

# Include canônico <Core/Foo.h> resolvido a partir de Public/ (ADR 0014).
target_include_directories(VibeCore
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private
)
