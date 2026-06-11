#pragma once

/**
 * @file Assert.h
 * @brief Macros de asserção `VASSERT`/`VVERIFY`/`VCHECK` e infraestrutura de relato (ADR 0009).
 * @details Ativação por preset (defines globais da ADR 0008): `VASSERT` ativo com `VIBE_ASSERTS=1`,
 *          `((void)0)` em Shipping; `VVERIFY` sempre avalia a expressão (1×); `VCHECK` ativo em todos
 *          os builds. Em build de teste (`VIBE_TESTING=1`) um handler instalável captura a falha sem
 *          abortar a suíte.
 */

namespace VibeEngine::Core::Detail
{
/**
 * @brief Relata uma falha de asserção.
 * @param Expr Texto da expressão que falhou.
 * @param File Arquivo de origem.
 * @param Line Linha de origem.
 * @param Message Mensagem literal associada (string vazia se ausente).
 * @details Em build de teste com handler instalado, chama o handler e retorna. Caso contrário,
 *          formata e envia ao hook de log (default `stderr`) e dispara `__debugbreak()`.
 */
void ReportAssertFailure(const char* Expr, const char* File, int Line, const char* Message);

/**
 * @brief Instala o hook de log usado pelo relato de asserção fora de build de teste.
 * @param Hook Função que recebe a mensagem já formatada (conectada ao `VLOG_ERROR` na T03).
 */
void SetAssertLogHook(void (*Hook)(const char* Formatted));
} // namespace VibeEngine::Core::Detail

#if VIBE_ASSERTS
/**
 * @brief Verifica `Expr`; em falha, relata (ativo apenas com `VIBE_ASSERTS=1`).
 * @param Expr Expressão booleana esperada verdadeira.
 */
#define VASSERT(Expr, ...)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(Expr))                                                                                                   \
        {                                                                                                              \
            ::VibeEngine::Core::Detail::ReportAssertFailure(#Expr, __FILE__, __LINE__, "" __VA_ARGS__);                \
        }                                                                                                              \
    } while (false)

/**
 * @brief Avalia `Expr` exatamente uma vez e relata em falha (check só com `VIBE_ASSERTS=1`).
 * @param Expr Expressão com efeito colateral que deve sempre executar.
 */
#define VVERIFY(Expr, ...)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(Expr))                                                                                                   \
        {                                                                                                              \
            ::VibeEngine::Core::Detail::ReportAssertFailure(#Expr, __FILE__, __LINE__, "" __VA_ARGS__);                \
        }                                                                                                              \
    } while (false)
#else
/**
 * @brief No-op em Shipping (`VIBE_ASSERTS=0`).
 */
#define VASSERT(Expr, ...) ((void)0)

/**
 * @brief Avalia `Expr` exatamente uma vez (sem check) em Shipping.
 * @param Expr Expressão com efeito colateral que deve sempre executar.
 */
#define VVERIFY(Expr, ...)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)(Expr);                                                                                                  \
    } while (false)
#endif

/**
 * @brief Invariante crítico ativo em TODOS os builds; relata em falha.
 * @param Expr Expressão booleana esperada verdadeira.
 */
#define VCHECK(Expr, ...)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(Expr))                                                                                                   \
        {                                                                                                              \
            ::VibeEngine::Core::Detail::ReportAssertFailure(#Expr, __FILE__, __LINE__, "" __VA_ARGS__);                \
        }                                                                                                              \
    } while (false)

#if VIBE_TESTING
namespace VibeEngine
{
/**
 * @brief Instala o handler de captura de asserção usado pelos testes.
 * @param Handler Ponteiro de função (sem captura) chamado em vez de abortar; `nullptr` desinstala.
 * @details Disponível apenas com `VIBE_TESTING=1`. Zero impacto em código de produção.
 */
void VASSERT_SetHandlerForTesting(void (*Handler)(const char* Expr, const char* File, int Line, const char* Message));
} // namespace VibeEngine
#endif
