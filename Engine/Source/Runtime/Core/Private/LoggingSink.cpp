#include <Core/Assert.h>
#include <Core/Logging.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

/**
 * @file LoggingSink.cpp
 * @brief Backend de logging (único TU que inclui spdlog — ADR 0006).
 */

namespace VibeEngine
{
namespace
{
/// @brief Estado de inicialização (idempotência).
bool g_Initialized = false;
/// @brief Nível mínimo que chega aos sinks.
LogLevel g_MinLevel = LogLevel::Info;

#if VIBE_TESTING
/// @brief Callback de captura instalado pelos testes (default `nullptr`).
void (*g_Capture)(LogLevel, const char*) = nullptr;
#endif

/// @brief Hook conectado a `Detail::SetAssertLogHook`: registra a falha como erro.
void AssertLogHook(const char* Formatted)
{
    Core::Detail::LogImpl(LogLevel::Error, Formatted);
}
} // namespace

void InitializeLogging()
{
    if (g_Initialized)
    {
        return;
    }
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::trace);
    g_MinLevel = LogLevel::Info;
    Core::Detail::SetAssertLogHook(&AssertLogHook);
    g_Initialized = true;
}

void ShutdownLogging()
{
    if (!g_Initialized)
    {
        return;
    }
    Core::Detail::SetAssertLogHook(nullptr);
    spdlog::default_logger()->flush();
    g_Initialized = false;
}

void SetLogLevel(LogLevel Minimum)
{
    g_MinLevel = Minimum;
}

#if VIBE_TESTING
void SetLogCaptureForTesting(void (*Capture)(LogLevel, const char*))
{
    g_Capture = Capture;
}
#endif

namespace Core::Detail
{
void LogImpl(LogLevel Level, const char* Message)
{
    if (static_cast<Vuint8>(Level) < static_cast<Vuint8>(g_MinLevel))
    {
        return;
    }

    switch (Level)
    {
    case LogLevel::Info:
        spdlog::info("{}", Message);
        break;
    case LogLevel::Warn:
        spdlog::warn("{}", Message);
        break;
    case LogLevel::Error:
        spdlog::error("{}", Message);
        break;
    }

#if VIBE_TESTING
    if (g_Capture != nullptr)
    {
        g_Capture(Level, Message);
    }
#endif
}
} // namespace Core::Detail
} // namespace VibeEngine
