#include "yqlora/physics/PhysicsMaterialFile.hpp"

#include "yqlora/core/FileFormat.hpp"
#include "yqlora/core/JsonFile.hpp"
#include "yqlora/reflection/PropertyJson.hpp"

#include <array>

namespace yqlora {

namespace {

using Json = nlohmann::json;

constexpr int kFirstPhysicsMaterialVersion = 1;
constexpr std::array<Migration, 0> kPhysicsMaterialMigrations{};
constexpr FileFormat kPhysicsMaterialFormat{
    "PhysicsMaterial",
    kFirstPhysicsMaterialVersion,
    kPhysicsMaterialMigrations
};

} // namespace

std::optional<PhysicsMaterialData> physicsMaterialFromJson(const Json& document, std::string& error)
{
    if (!document.is_object()) {
        error = "not a JSON object";

        return std::nullopt;
    }
    Json migrated = document;
    if (!kPhysicsMaterialFormat.migrate(migrated)) {
        error = "the format version is not supported";

        return std::nullopt;
    }
    const auto material = recordFromJson<PhysicsMaterialData>(migrated);
    if (!material) {
        error = "a field has a wrong value";

        return std::nullopt;
    }

    return sanitized(*material);
}

Json physicsMaterialToJson(const PhysicsMaterialData& material)
{
    Json document = recordToJson(material);
    kPhysicsMaterialFormat.stamp(document);

    return document;
}

bool writePhysicsMaterialFile(const std::filesystem::path& file, const PhysicsMaterialData& material)
{
    return writeJsonFile(file, physicsMaterialToJson(material));
}

} // namespace yqlora
