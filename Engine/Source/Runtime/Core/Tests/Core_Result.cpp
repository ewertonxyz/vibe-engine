#include <catch2/catch_test_macros.hpp>

#include <Core/Result.h>

#include <expected>
#include <string>

namespace
{
VibeEngine::VResult<int, std::string> Doubled(int Value)
{
    return Value * 2;
}
} // namespace

TEST_CASE("Core_Result_OkErrPropagation", "[core][unit]")
{
    using namespace VibeEngine;

    const VResult<int, std::string> Ok { 21 };
    REQUIRE(Ok.has_value());
    const auto Chained = Ok.and_then([](int Value) { return Doubled(Value); });
    REQUIRE(Chained.has_value());
    REQUIRE(Chained.value() == 42);

    const VResult<int, std::string> Err { std::unexpected(std::string { "nope" }) };
    REQUIRE_FALSE(Err.has_value());
    REQUIRE(Err.error() == "nope");

    const auto Propagated = Err.and_then([](int Value) { return Doubled(Value); });
    REQUIRE_FALSE(Propagated.has_value());
    REQUIRE(Propagated.error() == "nope");
}
