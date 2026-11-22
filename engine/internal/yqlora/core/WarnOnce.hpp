#pragma once

#include <cstdint>
#include <unordered_set>

namespace yqlora {

// Сообщение в лог об объекте (по его ID) один раз за жизнь владельца: то,
// что повторяется каждый шаг или кадр, не заливает лог.
class WarnOnce {
public:
    // true - об объекте id ещё не говорили, и теперь он помечен.
    bool first(std::uint64_t id)
    {
        return m_seen.insert(id).second;
    }

private:
    std::unordered_set<std::uint64_t> m_seen;
};

} // namespace yqlora
