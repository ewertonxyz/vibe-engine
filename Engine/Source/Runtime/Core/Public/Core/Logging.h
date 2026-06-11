#pragma once

#include <Core/Types.h>

/**
 * @file Logging.h
 * @brief Logging síncrono `VLOG_*` sobre spdlog (ADR 0006); no-op em Shipping (ADR 0008).
 */

namespace VibeEngine
{
/**
 * @brief Severidade de uma linha de log.
 */
enum class LogLevel : Vuint8
{
    Info, ///< @brief Informativo.
    Warn, ///< @brief Aviso.
    Error ///< @brief Erro.
};

/**
 * @brief Instala os sinks de log e conecta o hook de asserção (`Detail::SetAssertLogHook`).
 * @details Idempotente: uma segunda chamada é no-op. spdlog síncrono no MVP (ADR 0006).
 */
void InitializeLogging();

/**
 * @brief Faz flush e desinstala o logging.
 * @details Idempotente: chamar 2× é seguro. Desconecta o hook de asserção.
 */
void ShutdownLogging();

/**
 * @brief Define o nível mínimo que chega aos sinks.
 * @param Minimum Linhas com severidade abaixo de `Minimum` são descartadas.
 */
void SetLogLevel(LogLevel Minimum);

namespace Core::Detail
{
/**
 * @brief Encaminha uma mensagem já formada aos sinks (respeitando o nível mínimo).
 * @param Level Severidade da mensagem.
 * @param Message Texto já formatado (a macro não formata — MVP).
 */
void LogImpl(LogLevel Level, const char* Message);
} // namespace Core::Detail
} // namespace VibeEngine

#if VIBE_BUILD_SHIPPING
/**
 * @brief Loga uma mensagem informativa (no-op em Shipping).
 * @param Msg `const char*` já formatado.
 */
#define VLOG_INFO(Msg) ((void)0)

/**
 * @brief Loga um aviso (no-op em Shipping).
 * @param Msg `const char*` já formatado.
 */
#define VLOG_WARN(Msg) ((void)0)

/**
 * @brief Loga um erro (no-op em Shipping — ADR 0006/0008).
 * @param Msg `const char*` já formatado.
 */
#define VLOG_ERROR(Msg) ((void)0)
#else
/**
 * @brief Loga uma mensagem informativa.
 * @param Msg `const char*` já formatado.
 */
#define VLOG_INFO(Msg) ::VibeEngine::Core::Detail::LogImpl(::VibeEngine::LogLevel::Info, (Msg))

/**
 * @brief Loga um aviso.
 * @param Msg `const char*` já formatado.
 */
#define VLOG_WARN(Msg) ::VibeEngine::Core::Detail::LogImpl(::VibeEngine::LogLevel::Warn, (Msg))

/**
 * @brief Loga um erro.
 * @param Msg `const char*` já formatado.
 */
#define VLOG_ERROR(Msg) ::VibeEngine::Core::Detail::LogImpl(::VibeEngine::LogLevel::Error, (Msg))
#endif

#if VIBE_TESTING
namespace VibeEngine
{
/**
 * @brief Instala um callback de captura de log usado pelos testes (fixture `LogCapture`).
 * @param Capture Ponteiro de função que recebe cada linha que passa pelo filtro de nível;
 *                `nullptr` desinstala. Disponível apenas com `VIBE_TESTING=1` (precedente ADR 0009).
 */
void SetLogCaptureForTesting(void (*Capture)(LogLevel Level, const char* Message));
} // namespace VibeEngine
#endif
