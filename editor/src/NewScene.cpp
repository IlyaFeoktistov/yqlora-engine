#include "yqlora/editor/NewScene.hpp"

#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/DirectionalLight3D.hpp"
#include "yqlora/render/Environment.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora::editor {

namespace {

// Камера спереди и сверху от точки kNewSceneFocus.
const Vec3 kCameraPosition(0.0f, 2.0f, 5.0f);
// Свет над сеткой, где его значок видно, светит вниз и вбок: у куба три
// стороны освещены по-разному.
const Vec3 kLightPosition(0.0f, 3.0f, 0.0f);
const Vec3 kLightRotationDegrees(-50.0f, -30.0f, 0.0f);

} // namespace

void buildNewScene(Scene& scene, std::string rootName)
{
    auto& root = scene.createRoot<Node3D>(std::move(rootName));
    auto& camera = root.addChild<Camera3D>("Camera");
    camera.transform().setPosition(kCameraPosition);
    camera.transform().lookAt(kNewSceneFocus);

    auto& light = root.addChild<DirectionalLight3D>("Sun");
    light.transform().setPosition(kLightPosition);
    light.transform().setRotationDegrees(kLightRotationDegrees);
    light.setSun(true);

    // Небо, окружающий свет и атмосфера - со значениями по умолчанию.
    root.addChild<Environment>("Environment");
}

} // namespace yqlora::editor
