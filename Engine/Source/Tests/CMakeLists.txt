# Alvo único de testes da engine (Catch2). Cada módulo deixa seus testes em
# Engine/Source/Runtime/<Module>/Tests/*.cpp e eles são agregados aqui por glob
# (CONFIGURE_DEPENDS re-detecta arquivos novos a cada build) — ADR 0016.
file(GLOB VibeTestSources CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/Engine/Source/Runtime/*/Tests/*.cpp
)

add_executable(VibeTests ${VibeTestSources})

target_link_libraries(VibeTests PRIVATE Vibe::Core Catch2::Catch2WithMain)

# Copia as DLLs de runtime das dependências dinâmicas do vcpkg (spdlog, fmt, Tracy) ao lado do
# executável — a cópia applocal do vcpkg não cobre todas (ex.: TracyClient.dll). $<TARGET_RUNTIME_DLLS>
# resolve a lista completa das dependências SHARED importadas.
add_custom_command(TARGET VibeTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:VibeTests> $<TARGET_FILE_DIR:VibeTests>
    COMMAND_EXPAND_LISTS
)

# Descoberta automática de TEST_CASE para o ctest.
# DISCOVERY_MODE PRE_TEST: enumera os testes no tempo do ctest (não no build), garantindo que
# as DLLs de dependências dinâmicas do vcpkg (spdlog/fmt) já estejam ao lado do executável.
include(Catch)
catch_discover_tests(VibeTests DISCOVERY_MODE PRE_TEST)
