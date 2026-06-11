#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

/**
 * @file Types.h
 * @brief Aliases de tipos fundamentais da engine (V-prefix). Fonte: design-mvp.md §8.1, ADR 0001.
 */

namespace VibeEngine
{
/// @brief Inteiro com sinal de 8 bits.
using Vint8 = std::int8_t;
/// @brief Inteiro sem sinal de 8 bits.
using Vuint8 = std::uint8_t;
/// @brief Inteiro com sinal de 16 bits.
using Vint16 = std::int16_t;
/// @brief Inteiro sem sinal de 16 bits.
using Vuint16 = std::uint16_t;
/// @brief Inteiro com sinal de 32 bits.
using Vint32 = std::int32_t;
/// @brief Inteiro sem sinal de 32 bits.
using Vuint32 = std::uint32_t;
/// @brief Inteiro com sinal de 64 bits.
using Vint64 = std::int64_t;
/// @brief Inteiro sem sinal de 64 bits.
using Vuint64 = std::uint64_t;
/// @brief Ponto flutuante de precisão simples.
using Vfloat = float;
/// @brief Ponto flutuante de precisão dupla.
using Vdouble = double;
/// @brief Byte cru (não é caractere nem inteiro).
using Vbyte = std::byte;

/**
 * @brief View não-dona sobre uma sequência contígua de `T`.
 * @details Interfaces públicas recebem `Vspan`, nunca par ponteiro+tamanho (HARDENING §12).
 */
template <typename T>
using Vspan = std::span<T>;

/// @brief String de texto. Alias simples de `std::string` no MVP (ADR 0001).
using Vstring = std::string;
} // namespace VibeEngine
