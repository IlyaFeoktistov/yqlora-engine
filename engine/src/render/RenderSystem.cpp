#include "yqlora/render/RenderSystem.hpp"

#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/Light3D.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace yqlora {

std::vector<const Camera3D*> RenderSystem::cameras() const
{
    std::vector<const Camera3D*> result;
    std::ranges::copy_if(m_cameras, std::back_inserter(result), [](const Camera3D* camera) {
        return camera->activeInHierarchy();
    });
    std::ranges::stable_sort(result, std::less<>{}, &Camera3D::order);

    return result;
}

const Camera3D* RenderSystem::mainCamera() const
{
    const auto active = cameras();

    return active.empty() ? nullptr : active.front();
}

std::vector<const Light3D*> RenderSystem::lights() const
{
    std::vector<const Light3D*> result;
    std::ranges::copy_if(m_lights, std::back_inserter(result), [](const Light3D* light) {
        return light->activeInHierarchy();
    });

    return result;
}

bool RenderSystem::takeLightLimitReport() const
{
    return !std::exchange(m_lightLimitReported, true);
}

std::optional<int> orderToBecomeMain(const RenderSystem& render, const Camera3D& camera)
{
    const Camera3D* main = render.mainCamera();
    if (!camera.activeInHierarchy() || !main || main == &camera || main->order() == std::numeric_limits<int>::min()) {
        return std::nullopt;
    }

    return main->order() - 1;
}

void RenderSystem::addCamera(const Camera3D& camera)
{
    m_cameras.push_back(&camera);
}

void RenderSystem::removeCamera(const Camera3D& camera)
{
    std::erase(m_cameras, &camera);
}

void RenderSystem::addLight(const Light3D& light)
{
    m_lights.push_back(&light);
}

void RenderSystem::removeLight(const Light3D& light)
{
    std::erase(m_lights, &light);
}

} // namespace yqlora
