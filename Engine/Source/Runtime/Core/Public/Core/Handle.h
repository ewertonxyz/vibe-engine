#pragma once

#include <Core/Types.h>

#include <functional>

/**
 * @file Handle.h
 * @brief `VHandle<T>` — handle tipado com generation counter (ADR 0001).
 */

namespace VibeEngine
{
/**
 * @brief Handle opaco tipado de 8 bytes: índice + geração.
 * @details O generation counter detecta use-after-free: um índice reutilizado com
 *          geração nova é diferente do handle antigo. Geração 0 marca handle inválido.
 *          Cabe em registrador; comparável e hasheável (ADR 0001).
 * @tparam T Tag de tipo que distingue handles de recursos diferentes.
 */
template <typename T>
class alignas(8) VHandle
{
public:
    /// @brief Constrói um handle inválido (índice 0, geração 0).
    constexpr VHandle() noexcept = default;

    /**
     * @brief Constrói um handle a partir de índice e geração.
     * @param Index Índice do slot do recurso.
     * @param Generation Geração do slot; 0 é reservado para inválido.
     */
    constexpr VHandle(Vuint32 Index, Vuint32 Generation) noexcept
        : m_Index { Index }
        , m_Generation { Generation }
    {
    }

    /**
     * @brief Handle inválido canônico.
     * @return Handle com geração 0.
     */
    static constexpr VHandle Invalid() noexcept
    {
        return VHandle {};
    }

    /**
     * @brief Índice do slot.
     * @return O índice armazenado.
     */
    constexpr Vuint32 Index() const noexcept
    {
        return m_Index;
    }

    /**
     * @brief Geração do slot.
     * @return A geração armazenada.
     */
    constexpr Vuint32 Generation() const noexcept
    {
        return m_Generation;
    }

    /**
     * @brief Indica se o handle é válido.
     * @return `true` se a geração for diferente de 0.
     */
    constexpr bool IsValid() const noexcept
    {
        return m_Generation != 0;
    }

    /**
     * @brief Igualdade por valor (índice e geração).
     * @return `true` se ambos os campos coincidem.
     */
    constexpr bool operator==(const VHandle&) const noexcept = default;

private:
    /// @brief Índice do slot do recurso.
    Vuint32 m_Index { 0 };
    /// @brief Geração do slot; 0 = inválido.
    Vuint32 m_Generation { 0 };
};

static_assert(sizeof(VHandle<struct VHandleSizeProbe>) == 8);
} // namespace VibeEngine

/**
 * @brief Especialização de `std::hash` para `VHandle<T>`.
 * @details Combina geração e índice num único `Vuint64` antes de hashear.
 */
template <typename T>
struct std::hash<VibeEngine::VHandle<T>>
{
    /**
     * @brief Calcula o hash do handle.
     * @param Handle O handle a hashear.
     * @return Hash combinando geração (bits altos) e índice (bits baixos).
     */
    std::size_t operator()(const VibeEngine::VHandle<T>& Handle) const noexcept
    {
        const VibeEngine::Vuint64 Combined =
            (static_cast<VibeEngine::Vuint64>(Handle.Generation()) << 32) | Handle.Index();
        return std::hash<VibeEngine::Vuint64> {}(Combined);
    }
};
