#include "yqlora/editor/MaterialPreview.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/DirectionalLight3D.hpp"
#include "yqlora/render/Environment.hpp"
#include "yqlora/render/FrameBuilder.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/ResourceUses.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/resource/ResourceKinds.hpp"
#include "yqlora/scene/Node3D.hpp"

namespace yqlora::editor {

namespace {

// Камера смотрит на сферу (радиус 0.5 в начале координат) спереди, свет -
// сверху слева, чтобы были видны и блик, и тень на сфере. Свет, небо и
// экспозиция - по умолчанию, солнечный день.
const Vec3 kCameraPosition{0.0f, 0.0f, 1.6f};
const Vec3 kLightTarget{0.6f, -0.8f, -0.6f};

} // namespace

MaterialPreview::MaterialPreview(const EngineServices& services)
    : m_scene(services)
    , m_cache(services.resources())
{
    auto& root = m_scene.createRoot<Node3D>("Preview");
    auto& camera = root.addChild<Camera3D>("Camera");
    camera.transform().setPosition(kCameraPosition);
    auto& light = root.addChild<DirectionalLight3D>("Light");
    light.transform().lookAt(kLightTarget);
    light.setSun(true);
    m_environment = &root.addChild<Environment>("Environment");
    auto& sphere = root.addChild<Node3D>("Sphere");
    m_sphere = &sphere.addComponent<MeshRenderer>();
    m_sphere->setMesh({builtin::kSphereMesh});
    m_scene.updateTransforms();
}

FrameData MaterialPreview::frame(ResourceId material, float aspect)
{
    // Материал, годный и небу, и поверхности, - с неисправным шейдером:
    // на сфере он виден как ошибка.
    const bool sky = fitsResourceUse(uses::kSkyMaterial, material, m_cache)
                     && !fitsResourceUse(uses::kSurfaceMaterial, material, m_cache);
    m_environment->sky = {sky ? material : builtin::kDefaultSkyMaterial};
    m_sphere->setMaterials({{sky ? builtin::kDefaultMaterial : material}});

    return extractFrame(m_scene, aspect);
}

} // namespace yqlora::editor
