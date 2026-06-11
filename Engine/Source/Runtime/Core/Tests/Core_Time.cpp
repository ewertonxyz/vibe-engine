#include <catch2/catch_test_macros.hpp>

#include <Core/Time.h>

TEST_CASE("Core_Time_HighResClockMonotonic", "[core][unit]")
{
    using namespace VibeEngine;

    const Vuint64 First = HighResClock::NowTicks();
    const Vuint64 Second = HighResClock::NowTicks();
    REQUIRE(Second >= First);

    REQUIRE(HighResClock::TicksToSeconds(0) == 0.0);
}

TEST_CASE("Core_Time_FrameTimerDeltaNonNegative", "[core][unit]")
{
    using namespace VibeEngine;

    FrameTimer Timer;
    REQUIRE(Timer.FrameIndex() == 0);

    Timer.Tick();
    Timer.Tick();
    REQUIRE(Timer.DeltaSeconds() >= 0.0);
    REQUIRE(Timer.FrameIndex() == 2);
}
