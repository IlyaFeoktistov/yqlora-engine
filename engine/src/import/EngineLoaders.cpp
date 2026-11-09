#include "yqlora/import/EngineLoaders.hpp"

#include "yqlora/import/GltfLoader.hpp"
#include "yqlora/import/ImageLoader.hpp"
#include "yqlora/import/MaterialLoader.hpp"
#include "yqlora/import/SceneFileLoader.hpp"
#include "yqlora/import/ShaderLoader.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Prefab.hpp"
#include "yqlora/scene/SceneSerializer.hpp"

#include <memory>

namespace yqlora {

void addEngineLoaders(ResourceCache& cache)
{
    cache.addLoader(std::make_unique<GltfLoader>());
    cache.addLoader(std::make_unique<ImageLoader>());
    cache.addLoader(std::make_unique<MaterialLoader>());
    cache.addLoader(std::make_unique<ShaderLoader>());
    cache.addLoader(std::make_unique<SceneFileLoader>(SceneSerializer::fileExtension(), kinds::kScene));
    cache.addLoader(std::make_unique<SceneFileLoader>(kPrefabFileExtension, kinds::kPrefab));
}

} // namespace yqlora
