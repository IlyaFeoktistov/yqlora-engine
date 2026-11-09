#pragma once

#include "yqlora/reflection/Property.hpp"

#include <functional>
#include <span>

#include <string>

namespace yqlora {

class Component;
class Input;
class Node;
class Renderer;
class ResourceCache;
class ResourceDatabase;
class Scene;
class SettingsSection;
class TypeRegistry;

} // namespace yqlora

namespace yqlora::editor {

class Pings;

struct EditorColors;
struct EditorMetrics;

struct PropertyResult {
    // Свойство само не меняется: его записывают командой, чтобы правку можно было отменить.
    bool changed = false;
    PropertyValue value;
    // Значение тянут мышью: на это время стоит включить перенос курсора
    // у края окна, чтобы тянуть можно было бесконечно.
    bool dragging = false;
};

// Строка таблицы инспектора: подпись слева, значение справа. Вид поля
// выбирается по типу свойства и его подсказке.
//
// Числа меняются перетаскиванием подписи (у векторов - подписей X, Y, Z),
// а в само поле можно вписать значение. При перетаскивании Shift ускоряет
// в 10 раз, Ctrl замедляет в 10 раз.
// Что виджету нужно кроме самого свойства.
struct PropertyEditorContext {
    const EditorColors& colors;
    const EditorMetrics& metrics;
    // Из неё выбирается узел для ссылки на узел.
    const Scene& scene;
    // Из него выбирается тип компонента (PropertyHint::ComponentType).
    const TypeRegistry& registry;
    // Ввод приложения: привязка действия задаётся нажатием (PropertyHint::InputControlName).
    const Input& input;
    // Ресурсы проекта для ссылок на ресурсы. nullptr, если проекта нет.
    const ResourceDatabase* resources = nullptr;
    // Загруженные ресурсы: вид файла для списков и данные для превью.
    ResourceCache& cache;
    // Превью текстур.
    Renderer& renderer;
    // Имена слоёв по номеру (LayerSettings). Пусто без проекта: слои показываются номерами.
    std::span<const std::string> layerNames;
    // Открыть раздел Layers настроек проекта: пункт "Изменить слои..." в
    // списках слоёв. Пусто - пункта нет.
    std::function<void()> editLayers;
    // Показать цель ссылки в дереве и в "Файлах"; nullptr - поля только выбирают.
    Pings* pings = nullptr;
};

PropertyResult drawProperty(const Property& property, Node& node, const PropertyEditorContext& context);
PropertyResult drawProperty(const Property& property, Component& component, const PropertyEditorContext& context);
PropertyResult drawProperty(const Property& property, SettingsSection& section, const PropertyEditorContext& context);

// Подпись свойства: его displayName, иначе перевод ключа property.<имя>,
// а без перевода - имя с заглавной буквы.
std::string propertyLabel(const Property& property);

} // namespace yqlora::editor
