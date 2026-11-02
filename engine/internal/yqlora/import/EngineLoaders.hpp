#pragma once

namespace yqlora {

class ResourceCache;

// Загрузчики файлов, которые знает движок: glTF, картинки, шейдеры.
void addEngineLoaders(ResourceCache& cache);

} // namespace yqlora
