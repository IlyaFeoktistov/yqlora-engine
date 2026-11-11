#include <doctest/doctest.h>

#include <yqlora/platform/Process.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace yqlora;
using namespace yqlora::platform;

namespace {

// CMake есть везде, где собирается движок, и умеет простые команды (-E) одинаково на всех системах.
ProcessOptions cmake(std::vector<std::string> arguments)
{
    return {.program = YQLORA_CMAKE_COMMAND, .arguments = std::move(arguments), .workingDirectory = {}};
}

} // namespace

TEST_CASE("runProcess: output comes line by line, arguments with spaces and quotes stay whole")
{
    std::vector<std::string> lines;
    const auto result = runProcess(cmake({"-E", "echo", "two words", "a \"quoted\" one"}), [&](std::string_view line) {
        lines.emplace_back(line);
    });
    CHECK(result.succeeded());
    REQUIRE(lines.size() == 1);
    CHECK(lines.front() == "two words a \"quoted\" one");
}

TEST_CASE("runProcess: exit code, working directory and CRLF lines")
{
    const auto directory = std::filesystem::temp_directory_path() / "yqlora_tests" / "process";
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "lines.txt", std::ios::binary) << "first\r\nsecond\nlast";

    std::vector<std::string> lines;
    auto options = cmake({"-E", "cat", "lines.txt"});
    options.workingDirectory = directory;
    const auto result = runProcess(options, [&](std::string_view line) {
        lines.emplace_back(line);
    });
    CHECK(result.succeeded());
    CHECK(lines == std::vector<std::string>{"first", "second", "last"});

    const auto failed = runProcess(cmake({"-E", "false"}), {});
    CHECK(failed.started);
    CHECK_FALSE(failed.succeeded());
    CHECK(failed.exitCode != 0);
}

TEST_CASE("runProcess: a missing program does not start")
{
    const ProcessOptions options{.program = "/no/such/program", .arguments = {}, .workingDirectory = {}};
    const auto result = runProcess(options, {});
    CHECK_FALSE(result.succeeded());
    CHECK_FALSE(result.stopped);
}

TEST_CASE("runProcess: stop ends a running program")
{
    std::stop_source stop;
    std::thread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        stop.request_stop();
    });
    const auto started = std::chrono::steady_clock::now();
    const auto result = runProcess(cmake({"-E", "sleep", "30"}), {}, stop.get_token());
    stopper.join();
    CHECK(result.stopped);
    CHECK_FALSE(result.succeeded());
    CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(10));
}

TEST_CASE("launchDetached: the program runs without waiting, arguments with spaces stay whole")
{
    const auto directory = std::filesystem::temp_directory_path() / "yqlora_tests" / "launch";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto marker = directory / "two words.txt";

    auto options = cmake({"-E", "touch", "two words.txt"});
    options.workingDirectory = directory;
    const auto result = launchDetached(options);
    REQUIRE(result.started);

    // Программа живёт сама по себе: её конца никто не ждёт, файл появляется позже.
    constexpr auto kTimeout = std::chrono::seconds(10);
    constexpr auto kPoll = std::chrono::milliseconds(20);
    const auto started = std::chrono::steady_clock::now();
    while (!std::filesystem::exists(marker) && std::chrono::steady_clock::now() - started < kTimeout) {
        std::this_thread::sleep_for(kPoll);
    }
    CHECK(std::filesystem::exists(marker));
}

TEST_CASE("launchDetached: a missing program does not start")
{
    const ProcessOptions options{.program = "/no/such/program", .arguments = {}, .workingDirectory = {}};
    const auto result = launchDetached(options);
    CHECK_FALSE(result.started);
    CHECK_FALSE(result.error.empty());
}

#if defined(_WIN32)
TEST_CASE("launchDetached: batch files are refused")
{
    const ProcessOptions options{.program = "C:/tools/code.CMD", .arguments = {}, .workingDirectory = {}};
    const auto result = launchDetached(options);
    CHECK_FALSE(result.started);
    CHECK_FALSE(result.error.empty());
}
#endif
