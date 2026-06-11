# Módulo Core — tipos fundamentais da engine (ADR 0001/0009/0014).
add_library(VibeCore STATIC
    Private/StringIdTable.cpp
    Private/AssertImpl.cpp
    Private/LoggingSink.cpp
    Private/TimeImpl.cpp
)
add_library(Vibe::Core ALIAS VibeCore)

# Include canônico <Core/Foo.h> resolvido a partir de Public/ (ADR 0014).
target_include_directories(VibeCore
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

# spdlog: backend de logging confinado a LoggingSink.cpp (não vaza no header) -> PRIVATE.
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(VibeCore PRIVATE spdlog::spdlog)

# Tracy: linkado apenas quando o preset habilita TRACY_ENABLE (debug/development; ADR 0004/0008).
# PUBLIC porque Core/Profile.h inclui <tracy/Tracy.hpp> sob TRACY_ENABLE (consumidores precisam dos includes).
if(VIBE_DEFINES MATCHES "TRACY_ENABLE=1")
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(VibeCore PUBLIC Tracy::TracyClient)
endif()
