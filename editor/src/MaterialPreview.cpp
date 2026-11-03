#include "yqlora/editor/MaterialPreview.hpp"

#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/DirectionalLight3D.hpp"
#include "yqlora/render/FrameBuilder.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/scene/Node3D.hpp"

namespace yqlora::editor {

namespace {

// Камера смотрит на сферу (радиус 0.5 в начале координат) спереди, свет -
// сверху слева, чтобы были видны и блик, и тень на сфере.
const Vec3 kCameraPosition{0.0f, 0.0f, 1.6f};
const Vec3 kLightTarget{0.6f, -0.8f, -0.6f};
constexpr float kLightIntensity = 2.5f;

} // namespace

MaterialPreview::MaterialPreview(const EngineServices& services)
    : m_scene(services)
{
    auto& root = m_scene.createRoot<Node3D>("Preview");
    auto& camera = root.addChild<Camera3D>("Camera");
    camera.transform().setPosition(kCameraPosition);
    auto& light = root.addChild<DirectionalLight3D>("Light");
    light.setIntensity(kLightIntensity);
    light.transform().lookAt(kLightTarget);
    auto& sphere = root.addChild<Node3D>("Sphere");
    m_sphere = &sphere.addComponent<MeshRenderer>();
    m_sphere->setMesh({builtin::kSphereMesh});
    m_scene.updateTransforms();
}

FrameData MaterialPreview::frame(ResourceId material, float aspect)
{
    m_sphere->setMaterials({{material}});

    return extractFrame(m_scene, aspect);
}

} // namespace yqlora::editor
