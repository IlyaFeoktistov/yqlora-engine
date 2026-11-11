#include <doctest/doctest.h>

#include <yqlora/core/Log.hpp>
#include <yqlora/core/LogSubscription.hpp>
#include <yqlora/editor/LogBuffer.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

LogEntry entry(log::Level level, std::string message)
{
    return {.level = level, .source = LogSource::Build, .message = std::move(message), .directory = {}};
}

} // namespace

TEST_CASE("LogBuffer: entries written from another thread appear on collect")
{
    LogBuffer buffer;
    std::thread writer([&] {
        buffer.add(entry(log::Level::Error, "from a thread"));
    });
    writer.join();
    CHECK(buffer.entries().empty());

    CHECK(buffer.collect());
    REQUIRE(buffer.entries().size() == 1);
    CHECK(buffer.entries().front().message == "from a thread");
    CHECK(buffer.count(log::Level::Error) == 1);
    CHECK_FALSE(buffer.collect());
}

TEST_CASE("LogBuffer: the oldest entries leave over the capacity, counters follow")
{
    constexpr std::size_t kCapacity = 2;
    LogBuffer buffer(kCapacity);
    buffer.add(entry(log::Level::Error, "first"));
    buffer.add(entry(log::Level::Warning, "second"));
    buffer.add(entry(log::Level::Warning, "third"));
    buffer.collect();

    REQUIRE(buffer.entries().size() == kCapacity);
    CHECK(buffer.received() == 3);
    CHECK(buffer.entries().front().message == "second");
    CHECK(buffer.count(log::Level::Error) == 0);
    CHECK(buffer.count(log::Level::Warning) == 2);

    buffer.clear();
    CHECK(buffer.entries().empty());
    CHECK(buffer.count(log::Level::Warning) == 0);
}

TEST_CASE("LogFilter: levels and text without case")
{
    LogFilter filter;
    const LogEntry error = entry(log::Level::Error, "Player.cpp:12: error: no member");
    CHECK(filter.matches(error));

    filter.text = "PLAYER";
    CHECK(filter.matches(error));
    filter.text = "enemy";
    CHECK_FALSE(filter.matches(error));

    filter.text.clear();
    filter.levels[static_cast<std::size_t>(log::Level::Error)] = false;
    CHECK_FALSE(filter.matches(error));
}

TEST_CASE("Log subscription: the engine log reaches the buffer until the subscription goes")
{
    LogBuffer buffer;
    {
        const log::Subscription subscription = feedEngineLog(buffer);
        log::error("subscribed {}", 1);
    }
    log::error("not subscribed");
    buffer.collect();

    REQUIRE(buffer.entries().size() == 1);
    CHECK(buffer.entries().front().message == "subscribed 1");
    CHECK(buffer.entries().front().source == LogSource::Engine);
}

TEST_CASE("Log subscription: unsubscribing waits for a call already running")
{
    constexpr auto kCallTime = std::chrono::milliseconds(50);
    std::atomic<bool> inside{false};
    std::atomic<bool> finished{false};
    auto subscription = std::make_unique<log::Subscription>(log::subscribe([&](log::Level, std::string_view) {
        inside = true;
        std::this_thread::sleep_for(kCallTime);
        finished = true;
    }));
    std::thread writer([] {
        log::error("slow listener");
    });
    while (!inside) {
        std::this_thread::yield();
    }

    subscription.reset();
    CHECK(finished);
    writer.join();
}
