#pragma once

#include <optional>
#include <string>
#include <vector>

namespace yqlora {

class Component;
class Node;
class TypeInfo;

} // namespace yqlora

namespace yqlora::editor {

class EditorSession;

// Место камеры среди камер сцены. Главная - первая по order (см. RenderSystem).
enum class CameraRole {
    Main,
    Secondary,
    // Узел выключен: камера не рисует и главной стать не может.
    Inactive,
};

struct InspectedComponent {
    Component& component;
    // nullptr, если тип компонента не зарегистрирован.
    const TypeInfo* type = nullptr;
    // Имя для заголовка: у заглушки (MissingComponent) - имя пропавшего типа.
    std::string typeName;
    // См. canDetach в ComponentRules.hpp.
    bool removable = false;
    // Кому компонент нужен; пока они есть, его не снять.
    std::vector<const Component*> requiredBy;
};

// Что показывает инспектор для выбранного узла. Собирается без интерфейса,
// панель только рисует готовое и вызывает команды сессии.
struct Inspection {
    Node& node;
    const TypeInfo& type;
    std::vector<InspectedComponent> components;
    // nullopt, если узел не камера.
    std::optional<CameraRole> camera;
};

// nullopt, если ничего не выбрано или тип узла не зарегистрирован.
std::optional<Inspection> inspect(EditorSession& session);

} // namespace yqlora::editor
