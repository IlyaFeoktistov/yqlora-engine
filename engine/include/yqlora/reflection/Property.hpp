#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace yqlora {

class Node;
class Component;
class SettingsSection;

// Значение перечисления. В файлах сцен пишется именем, а не числом, поэтому
// порядок значений в C++ можно менять.
struct EnumValue {
    int value = 0;

    bool operator==(const EnumValue&) const = default;
};

// Ссылка на узел той же сцены. Если узел удалили, ссылка никуда не ведёт.
struct NodeRef {
    NodeId id = 0;

    bool operator==(const NodeRef&) const = default;
};

struct ResourceRef {
    ResourceId id = 0;

    bool operator==(const ResourceRef&) const = default;
};

struct ListValue;
struct RecordValue;

// Значение свойства, которое умеют показывать инспектор и сохранять файлы сцен.
// Порядок типов совпадает с PropertyType.
using PropertyValue = std::
    variant<bool, int, float, std::string, Vec2, Vec3, Vec4, EnumValue, NodeRef, ResourceRef, ListValue, RecordValue>;

// Список значений одного типа (Property::elementType). Элементы - тоже
// PropertyValue, поэтому список может хранить значения любого типа свойства.
struct ListValue {
    std::vector<PropertyValue> items;
};

// Запись: значения полей по порядку их описания (Property::fields, см. Record.hpp).
struct RecordValue {
    std::vector<PropertyValue> fields;
};

// После обеих структур: сравнение значений требует, чтобы все типы PropertyValue были полными.
inline bool operator==(const ListValue& a, const ListValue& b)
{
    return a.items == b.items;
}

inline bool operator==(const RecordValue& a, const RecordValue& b)
{
    return a.fields == b.fields;
}

enum class PropertyType {
    Bool,
    Int,
    Float,
    String,
    Vector2,
    Vector3,
    Vector4,
    Enum,
    Node,
    Resource,
    List,
    Record,
};

static_assert(std::variant_size_v<PropertyValue> == 12, "PropertyType must list every PropertyValue type");

// Как значение типа C++ хранится в PropertyValue. Обычно как есть,
// а любое перечисление C++ - как EnumValue.
template <typename T>
struct PropertyStorage {
    using Type = T;

    static const T& store(const T& value)
    {
        return value;
    }

    static const T& load(const T& value)
    {
        return value;
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct PropertyStorage<T> {
    using Type = EnumValue;

    static EnumValue store(T value)
    {
        return {static_cast<int>(value)};
    }

    static T load(const EnumValue& value)
    {
        return static_cast<T>(value.value);
    }
};

// 32 бита без знака (маска слоёв) - как int с теми же битами.
template <>
struct PropertyStorage<std::uint32_t> {
    using Type = int;

    static int store(std::uint32_t value)
    {
        return std::bit_cast<int>(value);
    }

    static std::uint32_t load(int value)
    {
        return std::bit_cast<std::uint32_t>(value);
    }
};

// Строка без владения (параметр метода) - как std::string.
template <>
struct PropertyStorage<std::string_view> {
    using Type = std::string;

    static std::string store(std::string_view value)
    {
        return std::string(value);
    }

    static std::string_view load(const std::string& value)
    {
        return value;
    }
};

// Указатель на узел (результат метода) - как ссылка на узел, nullptr - пустая
// ссылка. Обратно только через сцену, поэтому load нет: параметром метода
// указатель на узел быть не может.
template <typename T>
    requires std::derived_from<std::remove_const_t<T>, Node>
struct PropertyStorage<T*> {
    using Type = NodeRef;

    static NodeRef store(const T* node)
    {
        return node ? NodeRef{node->id()} : NodeRef{};
    }
};

// std::vector - как ListValue, каждый элемент хранится как свой тип.
template <typename T>
struct PropertyStorage<std::vector<T>> {
    using Type = ListValue;
    using Element = PropertyStorage<T>;

    static ListValue store(const std::vector<T>& values)
    {
        ListValue list;
        list.items.reserve(values.size());
        for (const T& value : values) {
            list.items.emplace_back(std::in_place_type<typename Element::Type>, Element::store(value));
        }

        return list;
    }

    static std::vector<T> load(const ListValue& list)
    {
        std::vector<T> values;
        values.reserve(list.items.size());
        for (const PropertyValue& item : list.items) {
            values.push_back(Element::load(std::get<typename Element::Type>(item)));
        }

        return values;
    }
};

template <typename T>
inline constexpr bool kIsListProperty = false;

template <typename T>
inline constexpr bool kIsListProperty<std::vector<T>> = true;

namespace detail {

template <typename T, typename Variant>
struct VariantIndex;

template <typename T, typename... Types>
struct VariantIndex<T, std::variant<Types...>> {
    static constexpr std::size_t find()
    {
        constexpr bool matches[] = {std::is_same_v<T, Types>...};
        for (std::size_t i = 0; i < sizeof...(Types); ++i) {
            if (matches[i]) {
                return i;
            }
        }

        return sizeof...(Types);
    }

    static constexpr std::size_t value = find();
    static_assert(value < sizeof...(Types), "Property type is not supported, see PropertyValue");
};

} // namespace detail

// PropertyType для типа C++: номер его вида хранения в PropertyValue.
// Для типа, которого нет в PropertyValue, не компилируется.
template <typename T>
constexpr PropertyType propertyTypeOf()
{
    using Stored = typename PropertyStorage<T>::Type;

    return static_cast<PropertyType>(detail::VariantIndex<Stored, PropertyValue>::value);
}

// Значение по умолчанию для типа: false, 0, пустая строка, пустая ссылка,
// пустой список, запись без полей. У записи настоящее значение по умолчанию
// знает её свойство (Property::defaultElement).
YQLORA_API PropertyValue defaultPropertyValue(PropertyType type);

// Как показывать свойство в инспекторе, если обычного поля для его типа мало.
enum class PropertyHint {
    None,
    Color,            // Vec3 или Vec4 как цвет RGB или RGBA с палитрой
    Angle,            // градусы: при перетаскивании значение меняется медленнее
    Layer,            // int: один слой узла из списка
    Layers,           // int: набор слоёв (LayerMask), флажок на каждый
    LayerNames,       // список строк: имена слоёв по номеру, длина постоянная, слой 0 не меняется
    ComponentType,    // строка: имя типа компонента, выбор из зарегистрированных
    InputControlName, // строка: имя клавиши или кнопки мыши, задаётся нажатием
};

// Как свойство показывается и сохраняется. Общее для всех языков, на которых
// описан тип (C++, Luau, графы): инспектор и файлы не знают, откуда поле.
struct PropertyAttributes {
    // Пределы чисел в инспекторе (int, float, числа векторов); nullopt - без предела.
    std::optional<float> min;
    std::optional<float> max;
    // Подпись в инспекторе; пусто - перевод property.<имя> или само имя.
    std::string displayName;
    // Подсказка при наведении: ключ перевода или сам текст.
    std::string tooltip;
    // Заголовок, под которым свойство стоит в инспекторе; пусто - без группы.
    std::string group;
    // Пишется в файл. false - значение только на время работы.
    bool saved = true;
    // Видно в инспекторе. false - только для кода и файла.
    bool shown = true;
    // Прежние имена: файл с таким именем читается, пишется уже новое.
    std::vector<std::string> formerly;
    // Вид ресурса, на который ведёт ссылка (у списка ссылок - его элементы):
    // T::kResourceKind. По нему инспектор предлагает ресурсы. Пусто - ссылка
    // только показывается.
    std::string resourceKind;
};

// Атрибуты в цепочке описания свойства: .range(0, 100).tooltip("..."). Одни
// и те же методы у строителя типа (к последнему свойству) и у полей скриптов
// и записей. Self даёт attributesToEdit().
template <typename Self>
class PropertyAttributeSetters {
public:
    Self& range(float min, float max)
    {
        edit().min = min;
        edit().max = max;

        return self();
    }

    Self& displayName(std::string name)
    {
        edit().displayName = std::move(name);

        return self();
    }

    Self& tooltip(std::string text)
    {
        edit().tooltip = std::move(text);

        return self();
    }

    Self& group(std::string name)
    {
        edit().group = std::move(name);

        return self();
    }

    Self& notSaved()
    {
        edit().saved = false;

        return self();
    }

    Self& notShown()
    {
        edit().shown = false;

        return self();
    }

    Self& formerly(std::string oldName)
    {
        edit().formerly.push_back(std::move(oldName));

        return self();
    }

    // .resource(MeshData::kResourceKind), см. PropertyAttributes::resourceKind.
    Self& resource(std::string_view kind)
    {
        edit().resourceKind = std::string(kind);

        return self();
    }

private:
    Self& self()
    {
        return static_cast<Self&>(*this);
    }

    PropertyAttributes& edit()
    {
        return self().attributesToEdit();
    }
};

// Свойство узла, компонента или раздела настроек проекта: имя, тип и доступ к значению. Доступ к
// значению - функции над PropertyValue, поэтому свойство может объявить и
// C++-класс (ClassTypeBuilder), и тип, описанный во время выполнения
// (скрипт Luau, граф визуального скрипта).
class YQLORA_API Property {
public:
    using NodeGetter = std::function<PropertyValue(const Node&)>;
    using NodeSetter = std::function<void(Node&, const PropertyValue&)>;
    using ComponentGetter = std::function<PropertyValue(const Component&)>;
    using ComponentSetter = std::function<void(Component&, const PropertyValue&)>;
    using SettingsGetter = std::function<PropertyValue(const SettingsSection&)>;
    using SettingsSetter = std::function<void(SettingsSection&, const PropertyValue&)>;
    // Подписи элементов списка у этого компонента (слоты материалов по
    // подмешам его меша): инспектор показывает строку на подпись, даже если
    // элементов меньше. Пустая подпись - "Элемент N".
    using ElementLabels = std::function<std::vector<std::string>(const Component&)>;

    // Setter получает значение уже проверенного типа type.
    static Property forNode(
        std::string name,
        PropertyType type,
        NodeGetter getter,
        NodeSetter setter,
        PropertyHint hint = PropertyHint::None
    );
    static Property forComponent(
        std::string name,
        PropertyType type,
        ComponentGetter getter,
        ComponentSetter setter,
        PropertyHint hint = PropertyHint::None
    );
    static Property forSettings(
        std::string name,
        PropertyType type,
        SettingsGetter getter,
        SettingsSetter setter,
        PropertyHint hint = PropertyHint::None
    );

    // Поле записи: только описание, без доступа к объекту (см. Record.hpp).
    static Property field(std::string name, PropertyType type, PropertyHint hint = PropertyHint::None);

    const std::string& name() const
    {
        return m_name;
    }

    PropertyType type() const
    {
        return m_type;
    }

    PropertyHint hint() const
    {
        return m_hint;
    }

    // Тип элементов списка (PropertyType::List). У остальных типов не важен.
    PropertyType elementType() const
    {
        return m_elementType;
    }

    const PropertyAttributes& attributes() const
    {
        return m_attributes;
    }

    // Атрибуты задают при описании типа (BasicTypeBuilder, поля скриптов и записей).
    PropertyAttributes& attributes()
    {
        return m_attributes;
    }

    // Имена значений перечисления по порядку. Пусто у остальных типов.
    const std::vector<std::string>& enumNames() const
    {
        return m_enumNames;
    }

    // Подписи элементов списка, см. ElementLabels; пусто - элементы по номерам.
    const ElementLabels& elementLabels() const
    {
        return m_elementLabels;
    }

    void setElementLabels(ElementLabels labels);

    // Поля записи по порядку: у свойства-записи - его, у списка записей - его
    // элементов. Пусто у остальных.
    const std::vector<Property>& fields() const
    {
        return m_fields;
    }

    // Новый элемент списка: запись со значениями полей из её C++-структуры,
    // остальное - defaultPropertyValue.
    PropertyValue defaultElement() const;

    // Объект должен быть того типа, для которого зарегистрировано свойство, или его наследником.
    PropertyValue get(const Node& node) const;
    PropertyValue get(const Component& component) const;
    PropertyValue get(const SettingsSection& section) const;

    // Значение того типа, что у свойства; у списка - и элементы того типа,
    // у записи - все поля по описанию.
    bool accepts(const PropertyValue& value) const;

    // false, если тип значения не совпадает с типом свойства.
    bool set(Node& node, const PropertyValue& value) const;
    bool set(Component& component, const PropertyValue& value) const;
    bool set(SettingsSection& section, const PropertyValue& value) const;

    // Имена значений перечисления по порядку, см. enumNames.
    void setEnumNames(std::vector<std::string> names);

    // Тип элементов списка, см. elementType.
    void setElementType(PropertyType type);

    // Поля записи (свойства-записи или элементов списка записей) и значения
    // полей по умолчанию, см. fields.
    void setRecord(std::vector<Property> fields, RecordValue defaults);

private:
    Property() = default;

    bool acceptsAs(PropertyType type, const PropertyValue& value) const;

    std::string m_name;
    PropertyType m_type = PropertyType::Bool;
    PropertyHint m_hint = PropertyHint::None;
    PropertyType m_elementType = PropertyType::Bool;
    std::vector<std::string> m_enumNames;
    std::vector<Property> m_fields;
    RecordValue m_recordDefaults;
    ElementLabels m_elementLabels;
    PropertyAttributes m_attributes;
    // Своё у каждого вида объектов: объект одного вида не передать свойству другого.
    NodeGetter m_nodeGet;
    NodeSetter m_nodeSet;
    ComponentGetter m_componentGet;
    ComponentSetter m_componentSet;
    SettingsGetter m_settingsGet;
    SettingsSetter m_settingsSet;
};

} // namespace yqlora
