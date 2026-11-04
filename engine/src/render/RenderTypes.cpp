#include "yqlora/render/RenderTypes.hpp"

#include "yqlora/material/MaterialData.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/DirectionalLight3D.hpp"
#include "yqlora/render/Environment.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/GraphicsSettings.hpp"
#include "yqlora/render/Light3D.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/PointLight3D.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/render/SpotLight3D.hpp"

namespace yqlora {

namespace {

// Пределы полей в инспекторе; сами классы держат только нижние пределы.
constexpr float kMaxShadowSoftness = 10.0f;
constexpr float kMaxShadowBias = 10.0f;
constexpr float kMaxAngularDiameter = 20.0f;
constexpr float kMinShadowDistance = 1.0f;
constexpr float kMaxShadowDistance = 10000.0f;
constexpr float kMinShadowMapSize = 256.0f;
constexpr float kMaxShadowMapSize = 8192.0f;

} // namespace

void registerRenderTypes(TypeRegistry& registry)
{
    registry.registerNode<Camera3D>()
        .property("fov", &Camera3D::fovDegrees, &Camera3D::setFovDegrees, PropertyHint::Angle)
        .property("near", &Camera3D::nearPlane, &Camera3D::setNearPlane)
        .property("far", &Camera3D::farPlane, &Camera3D::setFarPlane)
        .property(kCameraOrderProperty, &Camera3D::order, &Camera3D::setOrder)
        .property("cullingMask", &Camera3D::cullingMask, &Camera3D::setCullingMask, PropertyHint::Layers)
        .property("clear", &Camera3D::clear, &Camera3D::setClear)
        .property("background", &Camera3D::background, &Camera3D::setBackground, PropertyHint::Color)
        .property("exposure", &Camera3D::exposure, &Camera3D::setExposure)
        .property("toneMapping", &Camera3D::toneMapping, &Camera3D::setToneMapping);

    registry.registerNode<Light3D>()
        .property("color", &Light3D::color, &Light3D::setColor, PropertyHint::Color)
        .property("intensity", &Light3D::intensity, &Light3D::setIntensity)
        .property("shadows", &Light3D::shadows, &Light3D::setShadows)
        .group("group.shadows")
        .property("shadowStrength", &Light3D::shadowStrength, &Light3D::setShadowStrength)
        .range(0.0f, 1.0f)
        .group("group.shadows")
        .property("shadowSoftness", &Light3D::shadowSoftness, &Light3D::setShadowSoftness)
        .range(0.0f, kMaxShadowSoftness)
        .group("group.shadows")
        .property("shadowBias", &Light3D::shadowBias, &Light3D::setShadowBias)
        .range(0.0f, kMaxShadowBias)
        .group("group.shadows")
        .property("shadowNormalBias", &Light3D::shadowNormalBias, &Light3D::setShadowNormalBias)
        .range(0.0f, kMaxShadowBias)
        .group("group.shadows");
    registry.registerNode<DirectionalLight3D>()
        .property("sun", &DirectionalLight3D::sun, &DirectionalLight3D::setSun)
        .property(
            "angularDiameter",
            &DirectionalLight3D::angularDiameter,
            &DirectionalLight3D::setAngularDiameter,
            PropertyHint::Angle
        )
        .range(0.0f, kMaxAngularDiameter);
    registry.registerNode<PointLight3D>()
        .property("range", &PointLight3D::range, &PointLight3D::setRange)
        .property("sourceRadius", &PointLight3D::sourceRadius, &PointLight3D::setSourceRadius);
    registry.registerNode<SpotLight3D>()
        .property(
            "innerAngle",
            &SpotLight3D::innerAngleDegrees,
            &SpotLight3D::setInnerAngleDegrees,
            PropertyHint::Angle
        )
        .property(
            "outerAngle",
            &SpotLight3D::outerAngleDegrees,
            &SpotLight3D::setOuterAngleDegrees,
            PropertyHint::Angle
        );

    registry.registerNode<Environment>()
        .property("sky", &Environment::sky)
        .resource(MaterialData::kResourceKind, uses::kSkyMaterial)
        .property("ambientSource", &Environment::ambientSource)
        .group("group.ambientLight")
        .property("ambientIntensity", &Environment::ambientIntensity, &Environment::setAmbientIntensity)
        .group("group.ambientLight")
        .property("skyColor", &Environment::skyColor, PropertyHint::Color)
        .group("group.ambientLight")
        .property("equatorColor", &Environment::equatorColor, PropertyHint::Color)
        .group("group.ambientLight")
        .property("groundColor", &Environment::groundColor, PropertyHint::Color)
        .group("group.ambientLight")
        .property("ambientColor", &Environment::ambientColor, PropertyHint::Color)
        .group("group.ambientLight")
        .property("gradientLuminance", &Environment::gradientLuminance, &Environment::setGradientLuminance)
        .group("group.ambientLight")
        .property("atmosphere", &Environment::atmosphere);

    registry.registerComponent<MeshRenderer>()
        .requiresNode<Node3D>()
        .property("mesh", &MeshRenderer::mesh, &MeshRenderer::setMesh)
        .resource(MeshData::kResourceKind)
        .property(kMaterialsProperty, &MeshRenderer::materials, &MeshRenderer::setMaterials)
        .resource(MaterialData::kResourceKind, uses::kSurfaceMaterial)
        .elementLabels([](const Component& component) {
            return static_cast<const MeshRenderer&>(component).slotNames();
        })
        .property("castShadows", &MeshRenderer::castShadows)
        .property("receiveShadows", &MeshRenderer::receiveShadows);

    registry.registerSystem<RenderSystem>();

    registry.registerSettings<GraphicsSettings>("graphics")
        .property("windowsApi", &GraphicsSettings::windowsApi)
        .property("linuxApi", &GraphicsSettings::linuxApi)
        .property("vsync", &GraphicsSettings::vsync)
        .property("shadows", &GraphicsSettings::shadows)
        .group("group.shadows")
        .property("shadowDistance", &GraphicsSettings::shadowDistance)
        .range(kMinShadowDistance, kMaxShadowDistance)
        .group("group.shadows")
        .property("shadowCascades", &GraphicsSettings::shadowCascades)
        .range(1.0f, static_cast<float>(kMaxShadowCascades))
        .group("group.shadows")
        .property("shadowCascadeSize", &GraphicsSettings::shadowCascadeSize)
        .range(kMinShadowMapSize, kMaxShadowMapSize)
        .group("group.shadows")
        .property("shadowAtlasSize", &GraphicsSettings::shadowAtlasSize)
        .range(kMinShadowMapSize, kMaxShadowMapSize)
        .group("group.shadows")
        .property("shadowBlockerSamples", &GraphicsSettings::shadowBlockerSamples)
        .range(1.0f, static_cast<float>(kMaxShadowSamples))
        .group("group.shadows")
        .property("shadowFilterSamples", &GraphicsSettings::shadowFilterSamples)
        .range(1.0f, static_cast<float>(kMaxShadowSamples))
        .group("group.shadows")
        .property("shadowMaxPenumbra", &GraphicsSettings::shadowMaxPenumbra)
        .range(kMinShadowPenumbra, kMaxShadowPenumbra)
        .group("group.shadows");
}

} // namespace yqlora
