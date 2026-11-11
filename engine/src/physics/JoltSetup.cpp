// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/JoltSetup.hpp"

#include "yqlora/core/Log.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace yqlora {

namespace {

constexpr std::size_t kTraceBufferSize = 1024;

// Настройка Jolt глобальна в самом Jolt (Factory::sInstance, Trace), поэтому
// и счёт её держателей - один на процесс.
std::mutex g_mutex;
int g_holders = 0;

void trace(const char* format, ...)
{
    std::array<char, kTraceBufferSize> buffer{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(buffer.data(), buffer.size(), format, arguments);
    va_end(arguments);
    log::info("Jolt: {}", buffer.data());
}

#ifdef JPH_ENABLE_ASSERTS
// Проверка Jolt - ошибка программы, как наш assert: в лог, затем остановка.
bool assertFailed(const char* expression, const char* message, const char* file, JPH::uint line)
{
    log::error("Jolt assert '{}' {} ({}:{})", expression, message ? message : "", file, line);

    return true;
}
#endif

} // namespace

JoltSetup::JoltSetup()
{
    const std::lock_guard lock(g_mutex);
    if (g_holders++ > 0) {
        return;
    }
    JPH::RegisterDefaultAllocator();
    JPH::Trace = trace;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = assertFailed;)
    // Собраны с разными определениями (точность, проверки) - объекты Jolt
    // разной раскладки, работать нельзя.
    if (!JPH::VerifyJoltVersionID()) {
        log::error("Jolt library does not match the engine build (version or build defines)");
        std::abort();
    }
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

JoltSetup::~JoltSetup()
{
    const std::lock_guard lock(g_mutex);
    if (--g_holders > 0) {
        return;
    }
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

int JoltSetup::holders()
{
    const std::lock_guard lock(g_mutex);

    return g_holders;
}

} // namespace yqlora
