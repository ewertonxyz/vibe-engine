#pragma once

#include <Core/Types.h>

#include <cstddef>

/**
 * @file StringId.h
 * @brief `VStringId` — identificador estável FNV-1a 64-bit (ADR 0009).
 */

namespace VibeEngine::Core::Detail
{
/**
 * @brief Registra o texto original de um id na tabela reversa (apenas Debug/Development/Testing).
 * @param Hash Valor do `VStringId`.
 * @param Text Texto original.
 */
void RegisterStringId(Vuint64 Hash, Vspan<const char> Text) noexcept;

/**
 * @brief Busca o texto original de um id na tabela reversa.
 * @param Hash Valor do `VStringId`.
 * @return Texto original, ou "<unknown>" se ausente (sempre em Shipping).
 */
const char* LookupStringId(Vuint64 Hash) noexcept;
} // namespace VibeEngine::Core::Detail

namespace VibeEngine
{
/**
 * @brief Identificador de string compacto e estável (hash FNV-1a 64-bit sem seed).
 * @details Determinístico entre execuções, máquinas e plataformas (ADR 0009). Construído
 *          em compile-time para literais `_sid` ou em runtime a partir de um `Vspan`. Só o
 *          caminho runtime registra o texto na tabela reversa para `DebugString()`.
 */
class VStringId
{
public:
    /// @brief Constrói um id vazio (valor 0).
    constexpr VStringId() noexcept = default;

    /**
     * @brief Constrói o id a partir de um texto, calculando FNV-1a 64-bit.
     * @param Text Texto de origem (não precisa ser terminado em nulo).
     */
    explicit constexpr VStringId(Vspan<const char> Text) noexcept
    {
        Vuint64 Hash = 0xCBF29CE484222325ULL;
        for (const char Character : Text)
        {
            Hash ^= static_cast<Vuint64>(static_cast<unsigned char>(Character));
            Hash *= 0x100000001B3ULL;
        }
        m_Value = Hash;

        if !consteval
        {
            Core::Detail::RegisterStringId(m_Value, Text);
        }
    }

    /**
     * @brief Valor numérico do id.
     * @return O hash de 64 bits.
     */
    constexpr Vuint64 Value() const noexcept
    {
        return m_Value;
    }

    /**
     * @brief Texto original do id (diagnóstico).
     * @return O texto registrado, ou "<unknown>" para ids compile-time ou em Shipping.
     */
    const char* DebugString() const noexcept
    {
        return Core::Detail::LookupStringId(m_Value);
    }

    /**
     * @brief Igualdade por valor de hash.
     * @return `true` se os valores coincidem.
     */
    constexpr bool operator==(const VStringId&) const noexcept = default;

private:
    /// @brief Valor FNV-1a de 64 bits.
    Vuint64 m_Value { 0 };
};

/**
 * @brief Literal de usuário que constrói um `VStringId` em compile-time.
 * @param Str Ponteiro para o texto do literal.
 * @param Len Comprimento do literal.
 * @return O `VStringId` correspondente.
 */
constexpr VStringId operator""_sid(const char* Str, std::size_t Len) noexcept
{
    return VStringId { Vspan<const char> { Str, Len } };
}
} // namespace VibeEngine
