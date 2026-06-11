#include <Core/Time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/**
 * @file TimeImpl.cpp
 * @brief Implementação de `HighResClock`/`FrameTimer` (único TU com `<windows.h>`).
 */

namespace VibeEngine
{
namespace
{
/// @brief Frequência do contador de performance, cacheada na primeira chamada.
Vuint64 PerformanceFrequency() noexcept
{
    static const Vuint64 Frequency = []() noexcept -> Vuint64
    {
        LARGE_INTEGER Value {};
        QueryPerformanceFrequency(&Value);
        return static_cast<Vuint64>(Value.QuadPart);
    }();
    return Frequency;
}
} // namespace

Vuint64 HighResClock::NowTicks() noexcept
{
    LARGE_INTEGER Counter {};
    QueryPerformanceCounter(&Counter);
    return static_cast<Vuint64>(Counter.QuadPart);
}

Vdouble HighResClock::TicksToSeconds(Vuint64 Ticks) noexcept
{
    return static_cast<Vdouble>(Ticks) / static_cast<Vdouble>(PerformanceFrequency());
}

void FrameTimer::Tick() noexcept
{
    const Vuint64 Now = HighResClock::NowTicks();
    if (m_FrameIndex == 0)
    {
        m_DeltaSeconds = 0.0;
    }
    else
    {
        m_DeltaSeconds = HighResClock::TicksToSeconds(Now - m_LastTicks);
    }
    m_LastTicks = Now;
    ++m_FrameIndex;
}
} // namespace VibeEngine
