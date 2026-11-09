#pragma once

#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/reflection/Property.hpp"

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Запись - C++-структура, которую свойство хранит целиком: элемент действия
// ввода, строка порядка выполнения. Поля описываются один раз, рядом со
// структурой, вне пространств имён:
//
//   struct ExecutionOrderEntry {
//       std::string type;
//       int order = 0;
//   };
//
//   YQLORA_RECORD(
//       ExecutionOrderEntry,
//       field("type", &ExecutionOrderEntry::type, PropertyHint::ComponentType),
//       field("order", &ExecutionOrderEntry::order)
//   )
//
// Из этого описания реестр берёт поля для файлов и редактора; значение по
// умолчанию новой записи - ExecutionOrderEntry{}.
#define YQLORA_RECORD(Type, ...)                                                                                       \
    template <>                                                                                                        \
    struct yqlora::RecordFields<Type> {                                                                                \
        static auto fields()                                                                                           \
        {                                                                                                              \
            using ::yqlora::field;                                                                                     \
            using ::yqlora::PropertyHint;                                                                              \
            return std::tuple(__VA_ARGS__);                                                                            \
        }                                                                                                              \
    };

namespace yqlora {

// Поля записи T; определяет YQLORA_RECORD.
template <typename T>
struct RecordFields;

template <typename T>
concept Record = requires { RecordFields<T>::fields(); };

// Одно поле записи или скрипта: имя, член класса, подсказка редактору и
// атрибуты в цепочке: field(...).range(0, 100). Имена значений перечисления -
// из его таблицы, см. EnumNames.
template <typename Owner, typename Value>
struct RecordField : PropertyAttributeSetters<RecordField<Owner, Value>> {
    RecordField(std::string_view fieldName, Value Owner::* fieldMember, PropertyHint fieldHint)
        : name(fieldName)
        , member(fieldMember)
        , hint(fieldHint)
    {
    }

    PropertyAttributes& attributesToEdit()
    {
        return attributes;
    }

    std::string_view name;
    Value Owner::* member;
    PropertyHint hint = PropertyHint::None;
    PropertyAttributes attributes;
};

template <typename Owner, typename Value>
RecordField<Owner, Value> field(std::string_view name, Value Owner::* member, PropertyHint hint = PropertyHint::None)
{
    return {name, member, hint};
}

// Запись хранится как RecordValue: поля по порядку описания.
template <Record T>
struct PropertyStorage<T> {
    using Type = RecordValue;

    static RecordValue store(const T& value)
    {
        RecordValue record;
        std::apply(
            [&](const auto&... fields) {
                (record.fields.push_back(storeField(value.*(fields.member))), ...);
            },
            RecordFields<T>::fields()
        );

        return record;
    }

    static T load(const RecordValue& record)
    {
        T value{};
        std::size_t index = 0;
        std::apply(
            [&](const auto&... fields) {
                (loadField(value.*(fields.member), record.fields[index++]), ...);
            },
            RecordFields<T>::fields()
        );

        return value;
    }

private:
    template <typename Value>
    static PropertyValue storeField(const Value& value)
    {
        using Storage = PropertyStorage<Value>;

        return PropertyValue(std::in_place_type<typename Storage::Type>, Storage::store(value));
    }

    template <typename Value>
    static void loadField(Value& target, const PropertyValue& value)
    {
        using Storage = PropertyStorage<Value>;
        target = Storage::load(std::get<typename Storage::Type>(value));
    }
};

template <typename Value>
void describeValue(Property& property);

// Описания полей записи T для Property::setRecord.
template <Record T>
std::vector<Property> recordFieldsOf()
{
    std::vector<Property> result;
    std::apply(
        [&](const auto&... fields) {
            (result.push_back([&] {
                using Value = std::remove_cvref_t<decltype(std::declval<T>().*(fields.member))>;
                Property described = Property::field(std::string(fields.name), propertyTypeOf<Value>(), fields.hint);
                described.attributes() = fields.attributes;
                describeValue<Value>(described);

                return described;
            }()),
             ...);
        },
        RecordFields<T>::fields()
    );

    return result;
}

// Дописывает свойству со значением типа Value то, что не видно по одному
// PropertyType: имена значений перечисления, тип элементов списка и поля записи.
// Тип C++ ссылки говорит, на что она ведёт: вид ресурса Ref<T>, тип компонента
// ComponentRef<T>. У списка ссылок - его элементы.
template <typename Value>
inline constexpr bool kIsTypedResourceRef = false;
template <typename T>
inline constexpr bool kIsTypedResourceRef<Ref<T>> = true;

template <typename Value>
inline constexpr bool kIsTypedComponentRef = false;
template <typename T>
inline constexpr bool kIsTypedComponentRef<ComponentRef<T>> = true;

template <typename Value>
void describeReference(Property& property)
{
    if constexpr (kIsTypedResourceRef<Value>) {
        property.attributes().resourceKind = std::string(Value::Kind::kResourceKind);
    } else if constexpr (kIsTypedComponentRef<Value>) {
        property.attributes().componentType = std::string(Value::Target::staticTypeName());
    }
}

template <typename Value>
void describeValue(Property& property)
{
    static_assert(!std::is_enum_v<Value> || NamedEnum<Value>, "Enum property: add YQLORA_ENUM_NAMES for its enum");
    describeReference<Value>(property);
    if constexpr (kIsListProperty<Value>) {
        describeReference<typename Value::value_type>(property);
    }
    if constexpr (NamedEnum<Value>) {
        property.setEnumNames(
            std::vector<std::string>(std::begin(EnumNames<Value>::names), std::end(EnumNames<Value>::names))
        );
    } else if constexpr (kIsListProperty<Value>) {
        using Element = typename Value::value_type;
        property.setElementType(propertyTypeOf<Element>());
        if constexpr (Record<Element>) {
            property.setRecord(recordFieldsOf<Element>(), PropertyStorage<Element>::store(Element{}));
        }
    } else if constexpr (Record<Value>) {
        property.setRecord(recordFieldsOf<Value>(), PropertyStorage<Value>::store(Value{}));
    }
}

} // namespace yqlora
