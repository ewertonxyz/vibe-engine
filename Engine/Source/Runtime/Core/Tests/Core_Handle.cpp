#include <catch2/catch_test_macros.hpp>

#include <Core/Handle.h>

#include <functional>

namespace
{
struct HandleTag
{
};
} // namespace

TEST_CASE("Core_Handle_GenerationDistinguishesReuse", "[core][unit]")
{
    using namespace VibeEngine;

    const VHandle<HandleTag> First { 5, 1 };
    const VHandle<HandleTag> Second { 5, 2 };
    REQUIRE(First != Second);

    const std::hash<VHandle<HandleTag>> Hasher;
    REQUIRE(Hasher(First) != Hasher(Second));
}

TEST_CASE("Core_Handle_InvalidIsNotValid", "[core][unit]")
{
    using namespace VibeEngine;

    const VHandle<HandleTag> Default;
    REQUIRE_FALSE(Default.IsValid());
    REQUIRE_FALSE(VHandle<HandleTag>::Invalid().IsValid());

    REQUIRE(VHandle<HandleTag> { 0, 1 }.IsValid());

    const VHandle<HandleTag> Handle { 5, 7 };
    REQUIRE(Handle.Index() == 5);
    REQUIRE(Handle.Generation() == 7);
}
