#pragma once

#include <Core/Types.h>

/**
 * @file Time.h
 * @brief Relógio de alta resolução e timer de frame sobre `QueryPerformanceCounter`.
 */

namespace VibeEngine
{
/**
 * @brief Relógio monotônico de alta resolução (wrapper de `QueryPerformanceCounter`).
 */
class HighResClock
{
public:
    /**
     * @brief Lê o contador de performance atual.
     * @return Ticks monotônicos crescentes.
     */
    static Vuint64 NowTicks() noexcept;

    /**
     * @brief Converte ticks em segundos usando a frequência do contador (cacheada).
     * @param Ticks Quantidade de ticks.
     * @return Segundos equivalentes; `TicksToSeconds(0) == 0.0`.
     */
    static Vdouble TicksToSeconds(Vuint64 Ticks) noexcept;
};

/**
 * @brief Mede o delta entre frames e conta o índice do frame.
 * @details `DeltaSeconds()` é 0.0 até o segundo `Tick()`; `FrameIndex()` é 0 antes do primeiro.
 */
class FrameTimer
{
public:
    /**
     * @brief Avança um frame: atualiza o delta e incrementa o índice.
     */
    void Tick() noexcept;

    /**
     * @brief Delta do último frame.
     * @return Segundos desde o `Tick()` anterior (≥ 0.0; 0.0 antes do 2º `Tick()`).
     */
    Vdouble DeltaSeconds() const noexcept
    {
        return m_DeltaSeconds;
    }

    /**
     * @brief Índice do frame atual.
     * @return Número de `Tick()` já chamados (0 antes do 1º).
     */
    Vuint64 FrameIndex() const noexcept
    {
        return m_FrameIndex;
    }

private:
    /// @brief Ticks lidos no último `Tick()`.
    Vuint64 m_LastTicks { 0 };
    /// @brief Delta do último frame, em segundos.
    Vdouble m_DeltaSeconds { 0.0 };
    /// @brief Quantidade de `Tick()` chamados.
    Vuint64 m_FrameIndex { 0 };
};
} // namespace VibeEngine
