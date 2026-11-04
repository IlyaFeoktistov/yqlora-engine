#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/Property.hpp"

#include <functional>
#include <string_view>
#include <vector>

namespace yqlora {

class EngineServices;
struct MaterialData;

// Материал-ресурс для скриптов: изменение сразу видно у всех объектов с этим
// материалом и на диск не пишется. Один объект
// меняют замены (MeshRenderer::overrides). Значения хранятся по имени поля;
// подходит ли значение шейдеру, решается при отрисовке. Встроенные материалы
// не меняются.
class YQLORA_API SharedMaterial {
public:
    SharedMaterial(const EngineServices& services, ResourceRef material);

    // false, если материал не загружен или встроенный.
    bool setFloat(std::string_view name, float value);
    bool setVector(std::string_view name, const Vec2& value);
    bool setVector(std::string_view name, const Vec3& value);
    bool setVector(std::string_view name, const Vec4& value);
    // Только для полей с пометкой color; значение в sRGB, как в файле.
    bool setColor(std::string_view name, const Vec3& value);
    bool setColor(std::string_view name, const Vec4& value);
    bool setTexture(std::string_view name, ResourceRef texture);

private:
    bool change(const std::function<void(MaterialData&)>& edit);
    bool setNumbers(std::string_view name, std::vector<float> numbers);

    const EngineServices& m_services;
    ResourceRef m_material;
};

} // namespace yqlora
