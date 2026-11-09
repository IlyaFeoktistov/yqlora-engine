#include "yqlora/scene/ObjectId.hpp"

#include <random>

namespace yqlora {

std::uint64_t newObjectId()
{
    thread_local std::mt19937_64 engine{std::random_device{}()};
    std::uint64_t id = 0;
    while (id == 0) {
        id = engine();
    }

    return id;
}

} // namespace yqlora
