#include <catch2/catch_test_macros.hpp>

#include <Core/StringId.h>
#include <Core/Types.h>

#include <cstring>

TEST_CASE("Core_StringId_StableHashAndRoundtrip", "[core][unit]")
{
    using namespace VibeEngine;

    constexpr char Text[] = "Player";
    const VStringId Runtime { Vspan<const char> { Text, std::strlen(Text) } };

    // Hash não-trivial e estável entre runtime e literal _sid.
    REQUIRE(Runtime.Value() != 0);
    REQUIRE(Runtime.Value() == "Player"_sid.Value());

    // Roundtrip do texto para id construído em runtime.
    REQUIRE(std::strcmp(Runtime.DebugString(), "Player") == 0);

    // Literal avaliado em compile-time não entra na tabela reversa.
    constexpr VStringId Compiled = "CompileOnlyId"_sid;
    REQUIRE(std::strcmp(Compiled.DebugString(), "<unknown>") == 0);
}
