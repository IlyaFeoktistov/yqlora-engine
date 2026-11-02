#include "yqlora/import/EngineLoaders.hpp"

#include "yqlora/import/GltfLoader.hpp"
#include "yqlora/import/ImageLoader.hpp"
#include "yqlora/import/ShaderLoader.hpp"
#include "yqlora/resource/ResourceCache.hpp"

#include <memory>

namespace yqlora {

void addEngineLoaders(ResourceCache& cache)
{
    cache.addLoader(std::make_unique<GltfLoader>());
    cache.addLoader(std::make_unique<ImageLoader>());
    cache.addLoader(std::make_unique<ShaderLoader>());
}

} // namespace yqlora
