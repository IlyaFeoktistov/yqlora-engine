#pragma once

#include "yqlora/math/Math.hpp"

#include <optional>
#include <vector>

namespace yqlora {
class Scene;
struct Ray;
} // namespace yqlora

namespace yqlora::editor {

struct SnapSettings;

// Точка мира щелчком в окне сцены: инструмент (окно цепи) просит точку,
// окно сцены отвечает на щелчок. Один запрос за раз; владелец запроса -
// любой адрес, по которому инструмент узнаёт свой ответ (поле точки).
class PointPick {
public:
    // Новый запрос заменяет прежний. Ответ примет только та же сцена.
    void request(const void* owner, const Scene& scene);

    // Запрос owner ждёт ответа.
    bool waitingFor(const void* owner) const;

    // Есть запрос, на который сцена scene может ответить. Запрос другой
    // сцены (после Play или открытия другой сцены) отбрасывается.
    bool waitingIn(const Scene& scene);

    void cancel();

    // Ответ окна сцены на щелчок под лучом ray: точка на поверхности или
    // земле, с привязкой к сетке (см. dropPoint). false - ответ не принят:
    // запроса нет или он от другой сцены (тогда он отбрасывается).
    bool answer(const Scene& scene, const Ray& ray, const SnapSettings& snap);

    // Ответ на запрос owner, один раз; nullopt - ответа нет (отменён).
    std::optional<Vec3> take(const void* owner);

    // Линия, которую показывает окно сцены, пока инструмент открыт (путь
    // цепи): инструмент задаёт её каждый кадр, окно сцены забирает.
    void showGuide(std::vector<Vec3> line);
    std::vector<Vec3> takeGuide();

private:
    const void* m_owner = nullptr;
    const Scene* m_scene = nullptr;
    bool m_waiting = false;
    std::optional<Vec3> m_answer;
    std::vector<Vec3> m_guide;
};

} // namespace yqlora::editor
