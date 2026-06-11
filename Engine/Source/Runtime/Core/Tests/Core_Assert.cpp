#include <catch2/catch_test_macros.hpp>

#include <Core/Assert.h>

#include <cstring>

namespace
{
struct AssertCapture
{
    bool Fired { false };
    const char* Expr { nullptr };
    const char* File { nullptr };
    int Line { 0 };
    const char* Message { nullptr };
};

AssertCapture g_Capture;

void CaptureHandler(const char* Expr, const char* File, int Line, const char* Message)
{
    g_Capture.Fired = true;
    g_Capture.Expr = Expr;
    g_Capture.File = File;
    g_Capture.Line = Line;
    g_Capture.Message = Message;
}

int g_VerifyCalls { 0 };

bool VerifyReturnsFalse()
{
    ++g_VerifyCalls;
    return false;
}
} // namespace

TEST_CASE("Core_Assert_FailureCapturedInTestBuild", "[core][unit]")
{
    using namespace VibeEngine;

    g_Capture = AssertCapture {};
    VASSERT_SetHandlerForTesting(&CaptureHandler);

    SECTION("VASSERT captura expr, file, line e mensagem")
    {
        VASSERT(1 == 2, "boom");
        REQUIRE(g_Capture.Fired);
        REQUIRE(std::strcmp(g_Capture.Expr, "1 == 2") == 0);
        REQUIRE(std::strcmp(g_Capture.Message, "boom") == 0);
        REQUIRE(g_Capture.File != nullptr);
        REQUIRE(g_Capture.Line > 0);
    }

    SECTION("VVERIFY avalia a expressão exatamente 1x e dispara em falso")
    {
        g_VerifyCalls = 0;
        VVERIFY(VerifyReturnsFalse());
        REQUIRE(g_VerifyCalls == 1);
        REQUIRE(g_Capture.Fired);
    }

    SECTION("VCHECK dispara em falso")
    {
        VCHECK(false);
        REQUIRE(g_Capture.Fired);
    }

    VASSERT_SetHandlerForTesting(nullptr);
}
