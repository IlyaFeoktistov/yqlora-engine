#include "yqlora/render/RenderTypes.hpp"

#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/DirectionalLight3D.hpp"
#include "yqlora/render/GraphicsSettings.hpp"
#include "yqlora/render/Light3D.hpp"
#include "yqlora/render/LightingSettings.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/PointLight3D.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/render/SpotLight3D.hpp"

namespace yqlora {

void registerRenderTypes(TypeRegistry& registry)
{
    registry.registerNode<Camera3D>()
        .property("fov", &Camera3D::fovDegrees, &Camera3D::setFovDegrees, PropertyHint::Angle)
        .property("near", &Camera3D::nearPlane, &Camera3D::setNearPlane)
        .property("far", &Camera3D::farPlane, &Camera3D::setFarPlane)
        .property(kCameraOrderProperty, &Camera3D::order, &Camera3D::setOrder)
        .property("cullingMask", &Camera3D::cullingMask, &Camera3D::setCullingMask, PropertyHint::Layers)
        .property("clear", &Camera3D::clear, &Camera3D::setClear)
        .enumNames({"ColorAndDepth", "DepthOnly", "Nothing"})
        .property("background", &Camera3D::background, &Camera3D::setBackground, PropertyHint::Color);

    registry.registerNode<Light3D>()
        .property("color", &Light3D::color, &Light3D::setColor, PropertyHint::Color)
        .property("intensity", &Light3D::intensity, &Light3D::setIntensity);
    registry.registerNode<DirectionalLight3D>();
    registry.registerNode<PointLight3D>().property("range", &PointLight3D::range, &PointLight3D::setRange);
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

    registry.registerComponent<MeshRenderer>()
        .requiresNode<Node3D>()
        .property("mesh", &MeshRenderer::mesh, &MeshRenderer::setMesh, PropertyHint::Mesh)
        .property("color", &MeshRenderer::color, &MeshRenderer::setColor, PropertyHint::Color);

    registry.registerSystem<RenderSystem>();

    registry.registerSettings<GraphicsSettings>("graphics")
        .property("windowsApi", &GraphicsSettings::windowsApi)
        .enumNames(kGraphicsApiNames)
        .property("linuxApi", &GraphicsSettings::linuxApi)
        .enumNames(kGraphicsApiNames)
        .property("vsync", &GraphicsSettings::vsync);

    registry.registerSettings<LightingSettings>("lighting")
        .property("ambientColor", &LightingSettings::ambientColor, PropertyHint::Color)
        .property("ambientIntensity", &LightingSettings::ambientIntensity, &LightingSettings::setAmbientIntensity);
}

} // namespace yqlora
