#include "yqlora/editor/LogBuffer.hpp"

#include "yqlora/core/Text.hpp"

#include <algorithm>
#include <utility>

namespace yqlora::editor {

LogBuffer::LogBuffer(std::size_t capacity)
    : m_capacity(capacity)
{
}

void LogBuffer::add(LogEntry entry)
{
    const std::lock_guard lock(m_pendingMutex);
    m_pending.push_back(std::move(entry));
}

bool LogBuffer::collect()
{
    std::vector<LogEntry> arrived;
    {
        const std::lock_guard lock(m_pendingMutex);
        arrived.swap(m_pending);
    }
    m_received += arrived.size();
    for (LogEntry& entry : arrived) {
        ++m_counts[static_cast<std::size_t>(entry.level)];
        m_entries.push_back(std::move(entry));
    }
    while (m_entries.size() > m_capacity) {
        --m_counts[static_cast<std::size_t>(m_entries.front().level)];
        m_entries.pop_front();
    }

    return !arrived.empty();
}

void LogBuffer::clear()
{
    m_entries.clear();
    m_counts.fill(0);
}

bool LogFilter::matches(const LogEntry& entry) const
{
    return levels[static_cast<std::size_t>(entry.level)] && (text.empty() || containsIgnoringCase(entry.message, text));
}

log::Subscription feedEngineLog(LogBuffer& buffer)
{
    return log::subscribe([&buffer](log::Level level, std::string_view message) {
        buffer.add({.level = level, .source = LogSource::Engine, .message = std::string(message)});
    });
}

} // namespace yqlora::editor
