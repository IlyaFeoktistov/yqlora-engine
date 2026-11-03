#pragma once

#include "yqlora/resource/ResourceId.hpp"

#include <cstdint>
#include <iterator>
#include <map>
#include <memory>

namespace yqlora {

// Копии ресурсов в видеопамяти у бэкенда (меши, текстуры) по ID ресурса.
// Копия другой ревизии заменяется новой; копия, которой не пользовались
// kUnusedFramesToKeep кадров подряд, освобождается (ресурс перезагрузили,
// проект закрыли). Сама видеопамять - дело типа Gpu: так кэш одинаков у
// всех графических API. Revision - то, от чего зависит копия: ревизия
// ресурса или несколько ревизий (материал и его шейдер), сравнимые на равенство.
template <typename Gpu, typename Revision = std::uint64_t>
class GpuResourceCache {
public:
    static constexpr std::uint64_t kUnusedFramesToKeep = 120;

    // Копия ресурса id ревизии revision; create() создаёт новую, если копии
    // нет или она другой ревизии.
    template <typename Create>
    Gpu& get(ResourceId id, const Revision& revision, Create&& create)
    {
        Entry& entry = m_entries[id];
        if (!entry.gpu || !(entry.revision == revision)) {
            entry.gpu = create();
            entry.revision = revision;
        }
        entry.lastUsedFrame = m_frame;

        return *entry.gpu;
    }

    // Кадр показан: отпускает давно не нужные копии.
    void endFrame()
    {
        ++m_frame;
        for (auto it = m_entries.begin(); it != m_entries.end();) {
            it = m_frame - it->second.lastUsedFrame > kUnusedFramesToKeep ? m_entries.erase(it) : std::next(it);
        }
    }

    std::size_t size() const
    {
        return m_entries.size();
    }

private:
    struct Entry {
        Revision revision{};
        std::uint64_t lastUsedFrame = 0;
        std::unique_ptr<Gpu> gpu;
    };

    std::map<ResourceId, Entry> m_entries;
    std::uint64_t m_frame = 0;
};

} // namespace yqlora
