#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/scene/SceneSystem.hpp"

#include <optional>
#include <vector>

namespace yqlora {

class Camera3D;
class Light3D;

// Камеры сцены, через которые её видно, и источники света. Камеры рисуются
// по очереди, каждая со своей очисткой и маской слоёв; снимок кадра для
// рендерера собирает Extract (extractFrame во внутреннем API рендера).
class YQLORA_API RenderSystem final : public SceneSystem {
public:
    YQLORA_TYPE(RenderSystem, SceneSystem)

    using SceneSystem::SceneSystem;

    // Камеры с активным узлом в порядке отрисовки: меньший order раньше,
    // при равном - кто раньше вошёл в сцену.
    std::vector<const Camera3D*> cameras() const;

    // Главная камера - первая в порядке отрисовки, через неё видно игру.
    // nullptr, если активных камер нет.
    const Camera3D* mainCamera() const;

    // Источники с активным узлом, в порядке входа в сцену.
    std::vector<const Light3D*> lights() const;

    // Extract сообщает о лишних источниках один раз на сцену, а не каждый
    // кадр: true только при первом вызове.
    bool takeLightLimitReport() const;

private:
    // Камера сама входит в список, когда входит в сцену, и уходит вместе с ней.
    friend class Camera3D;
    void addCamera(const Camera3D& camera);
    void removeCamera(const Camera3D& camera);

    friend class Light3D;
    void addLight(const Light3D& light);
    void removeLight(const Light3D& light);

    std::vector<const Camera3D*> m_cameras;
    std::vector<const Light3D*> m_lights;
    // Снимок кадра читает сцену только для чтения, а сообщение - не состояние сцены.
    mutable bool m_lightLimitReported = false;
};

// Порядок, с которым camera станет главной: на единицу меньше, чем у первой
// камеры. Отдельного флага "главная" нет, чтобы он не спорил с порядком.
// nullopt, если камера уже главная, выключена или порядка меньше нет.
YQLORA_API std::optional<int> orderToBecomeMain(const RenderSystem& render, const Camera3D& camera);

} // namespace yqlora
