#pragma once

/**
 * @file Profile.h
 * @brief Macros de profiling Tracy (ADR 0004); no-op quando `TRACY_ENABLE` ausente.
 * @details Header macro-only. Inclui `<tracy/Tracy.hpp>` apenas sob `#if TRACY_ENABLE`,
 *          de modo que builds sem Tracy (shipping/asan-debug) não referenciam símbolo algum.
 */

#if TRACY_ENABLE
#include <tracy/Tracy.hpp>

/**
 * @brief Abre uma zona de profiling nomeada no escopo atual.
 * @param Name Literal de string com o nome da zona.
 */
#define VPROFILE_ZONE(Name) ZoneScopedN(Name)

/**
 * @brief Abre uma zona de profiling nomeada e colorida no escopo atual.
 * @param Name Literal de string com o nome da zona.
 * @param Color Cor RGB (`0xRRGGBB`).
 */
#define VPROFILE_ZONE_COLORED(Name, Color) ZoneScopedNC(Name, Color)

/**
 * @brief Marca o fim de um frame para o profiler.
 */
#define VPROFILE_FRAME() FrameMark

/**
 * @brief Publica um valor numérico nomeado no gráfico do profiler.
 * @param Name Literal de string com o nome da série.
 * @param Value Valor a publicar.
 */
#define VPROFILE_PLOT(Name, Value) TracyPlot(Name, Value)
#else
/**
 * @brief No-op sem `TRACY_ENABLE`.
 * @param Name Ignorado.
 */
#define VPROFILE_ZONE(Name) ((void)0)

/**
 * @brief No-op sem `TRACY_ENABLE`.
 * @param Name Ignorado.
 * @param Color Ignorado.
 */
#define VPROFILE_ZONE_COLORED(Name, Color) ((void)0)

/**
 * @brief No-op sem `TRACY_ENABLE`.
 */
#define VPROFILE_FRAME() ((void)0)

/**
 * @brief No-op sem `TRACY_ENABLE`.
 * @param Name Ignorado.
 * @param Value Ignorado.
 */
#define VPROFILE_PLOT(Name, Value) ((void)0)
#endif
