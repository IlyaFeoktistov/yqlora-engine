#include "yqlora/editor/PreviewLighting.hpp"

#include "yqlora/editor/NewScene.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora::editor {

PreviewLighting::PreviewLighting(const EngineServices& services)
    : m_services(services)
{
}

PreviewLighting::~PreviewLighting() = default;

const Scene& PreviewLighting::scene()
{
    if (!m_scene) {
        m_scene = std::make_unique<Scene>(m_services);
        addDefaultLighting(m_scene->createRoot<Node3D>("PreviewLighting"));
        // Направление солнца - из мировой матрицы узла.
        m_scene->updateTransforms();
    }

    return *m_scene;
}

} // namespace yqlora::editor
