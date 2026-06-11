#include <catch2/catch_test_macros.hpp>

#include <Core/Assert.h>
#include <Core/Handle.h>
#include <Core/Result.h>
#include <Core/StringId.h>
#include <Core/Types.h>

#include <expected>
#include <string>

namespace
{
struct SmokeTag
{
};

bool g_SmokeAssertFired = false;

void SmokeAssertHandler(const char*, const char*, int, const char*)
{
    g_SmokeAssertFired = true;
}
} // namespace

TEST_CASE("Core_Smoke_Task02", "[core][smoke]")
{
    using namespace VibeEngine;

    // Handle válido + reuse de índice com geração nova é distinto do antigo.
    const VHandle<SmokeTag> Old { 5, 1 };
    const VHandle<SmokeTag> Reused { 5, 2 };
    REQUIRE(Old.IsValid());
    REQUIRE(Old != Reused);

    // VResult ok/err.
    const VResult<int, std::string> Ok { 42 };
    const VResult<int, std::string> Err { std::unexpected(std::string { "bad" }) };
    REQUIRE(Ok.has_value());
    REQUIRE(Ok.value() == 42);
    REQUIRE_FALSE(Err.has_value());

    // VStringId estável.
    REQUIRE("Player"_sid == "Player"_sid);

    // VASSERT capturado por handler de teste.
    g_SmokeAssertFired = false;
    VASSERT_SetHandlerForTesting(&SmokeAssertHandler);
    VASSERT(false, "smoke");
    REQUIRE(g_SmokeAssertFired);
    VASSERT_SetHandlerForTesting(nullptr);
}
