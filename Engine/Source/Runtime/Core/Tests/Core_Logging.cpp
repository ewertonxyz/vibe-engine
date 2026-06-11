#include <catch2/catch_test_macros.hpp>

#include <Core/Assert.h>
#include <Core/Logging.h>
#include <Core/Profile.h>
#include <Core/Time.h>

#include <string>
#include <vector>

namespace
{
struct CapturedLine
{
    VibeEngine::LogLevel Level;
    std::string Message;
};

std::vector<CapturedLine> g_Captured;

void CaptureSink(VibeEngine::LogLevel Level, const char* Message)
{
    g_Captured.push_back({ Level, std::string(Message) });
}

/// @brief Fixture: instala o sink de captura no setup e o remove no teardown.
struct LogCapture
{
    LogCapture()
    {
        g_Captured.clear();
        VibeEngine::InitializeLogging();
        VibeEngine::SetLogLevel(VibeEngine::LogLevel::Info);
        VibeEngine::SetLogCaptureForTesting(&CaptureSink);
    }

    ~LogCapture()
    {
        VibeEngine::SetLogCaptureForTesting(nullptr);
        VibeEngine::SetLogLevel(VibeEngine::LogLevel::Info);
        VibeEngine::ShutdownLogging();
        g_Captured.clear();
    }
};

void AssertDelegatesToLog(const char*, const char*, int, const char* Message)
{
    VibeEngine::Core::Detail::LogImpl(VibeEngine::LogLevel::Error, Message);
}
} // namespace

TEST_CASE("Core_Smoke_Task03", "[core][smoke]")
{
    using namespace VibeEngine;

    g_Captured.clear();
    InitializeLogging();
    InitializeLogging(); // idempotente
    SetLogLevel(LogLevel::Info);
    SetLogCaptureForTesting(&CaptureSink);

    VLOG_INFO("hello");
    REQUIRE(g_Captured.size() == 1);

    FrameTimer Timer;
    Timer.Tick();
    Timer.Tick();
    REQUIRE(Timer.DeltaSeconds() >= 0.0);

    // Zonas em sub-blocos separados (ZoneScopedN usa nome de variável fixo).
    {
        VPROFILE_ZONE("SmokeZone");
    }
    {
        VPROFILE_ZONE_COLORED("SmokeZoneColored", 0xFF0000);
    }
    VPROFILE_FRAME();
    VPROFILE_PLOT("SmokePlot", 1.0);

    SetLogCaptureForTesting(nullptr);
    ShutdownLogging();
    ShutdownLogging(); // idempotente
    g_Captured.clear();
}

TEST_CASE("Core_Logging_LevelFilter", "[core][unit]")
{
    using namespace VibeEngine;
    LogCapture Capture;

    SetLogLevel(LogLevel::Error);
    VLOG_INFO("filtered out");
    REQUIRE(g_Captured.empty());

    VLOG_ERROR("kept");
    REQUIRE(g_Captured.size() == 1);
    REQUIRE(g_Captured[0].Level == LogLevel::Error);
}

TEST_CASE("Core_Logging_SinkReceivesFormatted", "[core][unit]")
{
    using namespace VibeEngine;
    LogCapture Capture;

    VLOG_WARN("exact message 123");
    REQUIRE(g_Captured.size() == 1);
    REQUIRE(g_Captured[0].Message == "exact message 123");
    REQUIRE(g_Captured[0].Level == LogLevel::Warn);
}

TEST_CASE("Core_Logging_AssertHookConnected", "[core][unit]")
{
    using namespace VibeEngine;
    LogCapture Capture;

    VASSERT_SetHandlerForTesting(&AssertDelegatesToLog);
    VASSERT(false, "boom");
    REQUIRE_FALSE(g_Captured.empty());
    REQUIRE(g_Captured.back().Message == "boom");
    REQUIRE(g_Captured.back().Level == LogLevel::Error);
    VASSERT_SetHandlerForTesting(nullptr);
}
