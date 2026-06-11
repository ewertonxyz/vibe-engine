#pragma once

#include <expected>

/**
 * @file Result.h
 * @brief `VResult<T,E>` — alias de `std::expected` (ADR 0001).
 */

namespace VibeEngine
{
/**
 * @brief Resultado falível: contém um `T` em sucesso ou um `E` em erro.
 * @details Alias de `std::expected<T,E>` (C++23). API: `has_value()`, `value()`,
 *          `error()`, `transform()`, `and_then()`. Hooks de log/telemetria ficam em
 *          camada externa, nunca no tipo (ADR 0001).
 */
template <typename T, typename E>
using VResult = std::expected<T, E>;
} // namespace VibeEngine
