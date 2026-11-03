#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/Property.hpp"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// Значение замены: числа по компонентам и то, цвет ли это (в sRGB).
struct MaterialOverrideValue {
    std::vector<float> numbers;
    bool color = false;

    bool operator==(const MaterialOverrideValue&) const = default;
};

// Замены полей материала у одного слота одного MeshRenderer, как
// MaterialPropertyBlock в Unity: общий материал (файл, данные в кэше,
// видеопамять) не меняется, поэтому другой объект с тем же материалом
// остаётся прежним. Со сценой не сохраняются. При отрисовке замена
// сверяется с полем шейдера по имени и типу: замена не того типа (шейдер
// перезагрузили, в слоте другой материал) пропускается и в другое поле не
// пишется.
class YQLORA_API MaterialOverrides {
public:
    void setFloat(std::string_view name, float value);
    void setVector(std::string_view name, const Vec2& value);
    void setVector(std::string_view name, const Vec3& value);
    void setVector(std::string_view name, const Vec4& value);
    // Только для полей с пометкой color; значение в sRGB, как в материале.
    void setColor(std::string_view name, const Vec4& value);
    // Пустая ссылка - текстура по умолчанию поля.
    void setTexture(std::string_view name, ResourceRef texture);

    // Убирает замену поля или текстуры: снова берётся значение материала.
    void clear(std::string_view name);
    void clearAll();

    bool empty() const
    {
        return m_values.empty() && m_textures.empty();
    }

    const std::map<std::string, MaterialOverrideValue, std::less<>>& values() const
    {
        return m_values;
    }

    const std::map<std::string, ResourceRef, std::less<>>& textures() const
    {
        return m_textures;
    }

    bool operator==(const MaterialOverrides&) const = default;

private:
    void set(std::string_view name, MaterialOverrideValue value);

    std::map<std::string, MaterialOverrideValue, std::less<>> m_values;
    std::map<std::string, ResourceRef, std::less<>> m_textures;
};

} // namespace yqlora
