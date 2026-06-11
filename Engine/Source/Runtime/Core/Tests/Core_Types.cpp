#include <catch2/catch_test_macros.hpp>

#include <Core/Types.h>

#include <string>

TEST_CASE("Core_Types_FixedSizes", "[core][unit]")
{
    using namespace VibeEngine;

    STATIC_REQUIRE(sizeof(Vint8) == 1);
    STATIC_REQUIRE(sizeof(Vuint8) == 1);
    STATIC_REQUIRE(sizeof(Vint16) == 2);
    STATIC_REQUIRE(sizeof(Vuint16) == 2);
    STATIC_REQUIRE(sizeof(Vint32) == 4);
    STATIC_REQUIRE(sizeof(Vuint32) == 4);
    STATIC_REQUIRE(sizeof(Vint64) == 8);
    STATIC_REQUIRE(sizeof(Vuint64) == 8);
    STATIC_REQUIRE(sizeof(Vfloat) == 4);
    STATIC_REQUIRE(sizeof(Vdouble) == 8);
    STATIC_REQUIRE(sizeof(Vbyte) == 1);

    constexpr char Literal[] = "abcd";
    const Vspan<const char> Span { Literal, 4 };
    REQUIRE(Span.size() == 4);

    const Vstring Text { "hello" };
    REQUIRE(Text == "hello");
}
