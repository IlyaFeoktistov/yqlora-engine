#pragma once

#include <memory>

namespace yqlora {
class EngineServices;
class Scene;
} // namespace yqlora

namespace yqlora::editor {

// Свет предпросмотра префаба. Префаб - объект без своего места: неба и
// солнца у него обычно нет, и в окне сцены он был бы в темноте. Здесь -
// скрытая сцена с солнцем и окружением новой сцены (addDefaultLighting);
// окно сцены берёт из неё то, чего нет у префаба (extractFrame). В файл
// префаба ничего не попадает.
class PreviewLighting {
public:
    explicit PreviewLighting(const EngineServices& services);
    ~PreviewLighting();

    PreviewLighting(const PreviewLighting&) = delete;
    PreviewLighting& operator=(const PreviewLighting&) = delete;

    // Сцена со светом; строится при первом обращении.
    const Scene& scene();

private:
    const EngineServices& m_services;
    std::unique_ptr<Scene> m_scene;
};

} // namespace yqlora::editor
