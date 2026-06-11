# Alvo único de testes da engine (Catch2). Nasce na T02; novos módulos acrescentam fontes aqui.
add_executable(VibeTests
    ${CMAKE_SOURCE_DIR}/Engine/Source/Runtime/Core/Tests/Core_Smoke.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Runtime/Core/Tests/Core_Types.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Runtime/Core/Tests/Core_Handle.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Runtime/Core/Tests/Core_Result.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Runtime/Core/Tests/Core_StringId.cpp
    ${CMAKE_SOURCE_DIR}/Engine/Source/Runtime/Core/Tests/Core_Assert.cpp
)

target_link_libraries(VibeTests PRIVATE Vibe::Core Catch2::Catch2WithMain)

# Descoberta automática de TEST_CASE para o ctest.
include(Catch)
catch_discover_tests(VibeTests)
