#include "yqlora/material/MaterialFile.hpp"

#include "yqlora/core/FileFormat.hpp"
#include "yqlora/core/JsonFile.hpp"
#include "yqlora/core/JsonId.hpp"

#include <array>

namespace yqlora {

namespace {

using Json = nlohmann::json;

constexpr const char* kShaderKey = "shader";
constexpr const char* kBlendKey = "blend";
constexpr const char* kCullKey = "cull";
constexpr const char* kValuesKey = "parameters";
constexpr const char* kTexturesKey = "textures";

constexpr int kFirstMaterialVersion = 1;
constexpr std::array<Migration, 0> kMaterialMigrations{};
constexpr FileFormat kMaterialFormat{"Material", kFirstMaterialVersion, kMaterialMigrations};

// Число поля - одно число, вектор - массив чисел.
std::optional<std::vector<float>> numbersFromJson(const Json& json)
{
    if (json.is_number()) {
        return std::vector<float>{json.get<float>()};
    }
    if (!json.is_array() || json.empty()) {
        return std::nullopt;
    }
    std::vector<float> numbers;
    for (const Json& item : json) {
        if (!item.is_number()) {
            return std::nullopt;
        }
        numbers.push_back(item.get<float>());
    }

    return numbers;
}

template <NamedEnum E>
bool readEnum(const Json& document, const char* key, E& value, std::string& error)
{
    const auto it = document.find(key);
    if (it == document.end()) {
        return true;
    }
    const auto parsed = it->is_string() ? enumFromName<E>(it->get<std::string>()) : std::nullopt;
    if (!parsed) {
        error = std::string("unknown ") + key + " " + it->dump();

        return false;
    }
    value = *parsed;

    return true;
}

} // namespace

std::optional<MaterialData> materialFromJson(const Json& document, std::string& error)
{
    if (!document.is_object()) {
        error = "not a JSON object";

        return std::nullopt;
    }
    Json migrated = document;
    if (!kMaterialFormat.migrate(migrated)) {
        error = "the format version is not supported";

        return std::nullopt;
    }
    MaterialData material;
    if (const auto shader = migrated.find(kShaderKey); shader != migrated.end()) {
        material.shader = idFromJson(*shader).value_or(0);
    }
    if (!readEnum(migrated, kBlendKey, material.blend, error) || !readEnum(migrated, kCullKey, material.cull, error)) {
        return std::nullopt;
    }
    if (const auto values = migrated.find(kValuesKey); values != migrated.end() && values->is_object()) {
        for (const auto& [name, value] : values->items()) {
            if (auto numbers = numbersFromJson(value)) {
                material.values[name] = std::move(*numbers);
            }
        }
    }
    if (const auto textures = migrated.find(kTexturesKey); textures != migrated.end() && textures->is_object()) {
        for (const auto& [name, value] : textures->items()) {
            if (const auto id = idFromJson(value)) {
                material.textures[name] = *id;
            }
        }
    }

    return material;
}

Json materialToJson(const MaterialData& material)
{
    Json document;
    kMaterialFormat.stamp(document);
    document[kShaderKey] = idToJson(material.shader);
    document[kBlendKey] = enumName(material.blend);
    document[kCullKey] = enumName(material.cull);
    Json values = Json::object();
    for (const auto& [name, numbers] : material.values) {
        values[name] = numbers.size() == 1 ? Json(numbers.front()) : Json(numbers);
    }
    document[kValuesKey] = std::move(values);
    Json textures = Json::object();
    for (const auto& [name, id] : material.textures) {
        textures[name] = idToJson(id);
    }
    document[kTexturesKey] = std::move(textures);

    return document;
}

bool writeMaterialFile(const std::filesystem::path& file, const MaterialData& material)
{
    return writeJsonFile(file, materialToJson(material));
}

} // namespace yqlora
