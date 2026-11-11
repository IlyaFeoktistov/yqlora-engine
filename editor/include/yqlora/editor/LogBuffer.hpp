#pragma once

#include "yqlora/core/Log.hpp"
#include "yqlora/core/LogSubscription.hpp"

#include <array>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace yqlora::editor {

// Откуда запись: сообщения движка и редактора или вывод сборки скриптов.
enum class LogSource {
    Engine,
    Build,
};

struct LogEntry {
    log::Level level = log::Level::Info;
    LogSource source = LogSource::Engine;
    std::string message;
    // Откуда считаются относительные пути "файл:строка" в тексте; пусто - от
    // папки проекта.
    std::filesystem::path directory;
};

inline constexpr std::size_t kLogLevelCount = static_cast<std::size_t>(log::Level::Error) + 1;

// Последние записи лога для панели "Лог". Писать можно из любого потока
// (лог движка, сборка), а читать - только в главном: новые записи попадают в
// entries() при collect(), поэтому интерфейс не видит список посреди изменения.
class LogBuffer {
public:
    static constexpr std::size_t kDefaultCapacity = 5000;

    explicit LogBuffer(std::size_t capacity = kDefaultCapacity);

    LogBuffer(const LogBuffer&) = delete;
    LogBuffer& operator=(const LogBuffer&) = delete;

    // Из любого потока.
    void add(LogEntry entry);

    // Главный поток: переносит пришедшие записи; самые старые сверх ёмкости
    // уходят. true, если что-то пришло.
    bool collect();

    const std::deque<LogEntry>& entries() const
    {
        return m_entries;
    }

    // Сколько записей уровня level среди entries().
    std::size_t count(log::Level level) const
    {
        return m_counts[static_cast<std::size_t>(level)];
    }

    // Сколько записей пришло за всё время, включая ушедшие и очищенные: растёт
    // с каждой новой, даже когда entries() уже полон.
    std::size_t received() const
    {
        return m_received;
    }

    void clear();

private:
    std::mutex m_pendingMutex;
    std::vector<LogEntry> m_pending;
    std::deque<LogEntry> m_entries;
    std::array<std::size_t, kLogLevelCount> m_counts{};
    std::size_t m_capacity;
    std::size_t m_received = 0;
};

// Что панель "Лог" показывает: выбранные уровни и записи с текстом (без учёта
// регистра латиницы).
struct LogFilter {
    // Сначала видны все уровни, сколько бы их ни было.
    std::array<bool, kLogLevelCount> levels = [] {
        std::array<bool, kLogLevelCount> all{};
        all.fill(true);

        return all;
    }();
    std::string text;

    bool matches(const LogEntry& entry) const;
};

// Сообщения лога движка идут в buffer, пока жива подписка.
log::Subscription feedEngineLog(LogBuffer& buffer);

} // namespace yqlora::editor
