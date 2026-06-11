#include <Core/Assert.h>

#include <cstdio>
#include <cstdlib>

/**
 * @file AssertImpl.cpp
 * @brief Implementação do relato de asserção e do handler de teste (ADR 0009).
 */

namespace VibeEngine::Core::Detail
{
namespace
{
/// @brief Hook de log instalável (default `nullptr` → usa `stderr`).
void (*g_LogHook)(const char*) = nullptr;

#if VIBE_TESTING
/// @brief Handler de captura instalado pelos testes (default `nullptr`).
void (*g_TestHandler)(const char*, const char*, int, const char*) = nullptr;
#endif
} // namespace

void SetAssertLogHook(void (*Hook)(const char* Formatted))
{
    g_LogHook = Hook;
}

void ReportAssertFailure(const char* Expr, const char* File, int Line, const char* Message)
{
#if VIBE_TESTING
    if (g_TestHandler != nullptr)
    {
        g_TestHandler(Expr, File, Line, Message);
        return;
    }
#endif

    char Formatted[1024];
    std::snprintf(Formatted, sizeof(Formatted), "Assertion failed: %s | %s:%d | %s", Expr, File, Line, Message);

    if (g_LogHook != nullptr)
    {
        g_LogHook(Formatted);
    }
    else
    {
        std::fprintf(stderr, "%s\n", Formatted);
    }

#if defined(_MSC_VER)
    __debugbreak();
#else
    std::abort();
#endif
}
} // namespace VibeEngine::Core::Detail

#if VIBE_TESTING
namespace VibeEngine
{
void VASSERT_SetHandlerForTesting(void (*Handler)(const char* Expr, const char* File, int Line, const char* Message))
{
    Core::Detail::g_TestHandler = Handler;
}
} // namespace VibeEngine
#endif
