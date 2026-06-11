#include <Core/StringId.h>

#include <string>
#include <unordered_map>

/**
 * @file StringIdTable.cpp
 * @brief Tabela reversa `Vuint64 -> texto` para `VStringId::DebugString()` (ADR 0009).
 * @details Presente apenas em Debug/Development/Testing; em Shipping vira no-op de custo zero.
 */

namespace VibeEngine::Core::Detail
{
#if VIBE_TESTING || VIBE_BUILD_DEBUG || VIBE_BUILD_DEVELOPMENT
namespace
{
/// @brief Acesso à tabela reversa global (lazy, lifetime estático).
std::unordered_map<Vuint64, Vstring>& Table() noexcept
{
    static std::unordered_map<Vuint64, Vstring> Instance;
    return Instance;
}
} // namespace

void RegisterStringId(Vuint64 Hash, Vspan<const char> Text) noexcept
{
    Table().try_emplace(Hash, Vstring { Text.data(), Text.size() });
}

const char* LookupStringId(Vuint64 Hash) noexcept
{
    const auto It = Table().find(Hash);
    if (It != Table().end())
    {
        return It->second.c_str();
    }
    return "<unknown>";
}
#else
void RegisterStringId(Vuint64 Hash, Vspan<const char> Text) noexcept
{
    (void)Hash;
    (void)Text;
}

const char* LookupStringId(Vuint64 Hash) noexcept
{
    (void)Hash;
    return "<unknown>";
}
#endif
} // namespace VibeEngine::Core::Detail
