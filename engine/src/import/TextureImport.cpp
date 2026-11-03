#include "yqlora/import/TextureImport.hpp"

#include "yqlora/core/Log.hpp"

#include <string>

namespace yqlora {

namespace {

constexpr const char* kColorSpaceKey = "colorSpace";
constexpr const char* kFilterKey = "filter";
constexpr const char* kWrapKey = "wrap";
constexpr const char* kMipmapsKey = "mipmaps";

template <NamedEnum E>
void readEnum(const nlohmann::json& import, const char* key, E& value)
{
    const auto it = import.find(key);
    if (it == import.end()) {
        return;
    }
    const auto found = it->is_string() ? enumFromName<E>(it->get<std::string>()) : std::nullopt;
    if (!found) {
        log::warn("Texture import setting '{}' has an unknown value {}, using the default", key, it->dump());

        return;
    }
    value = *found;
}

} // namespace

TextureSampling textureSamplingFromJson(const nlohmann::json& import)
{
    TextureSampling sampling;
    if (!import.is_object()) {
        return sampling;
    }
    readEnum(import, kColorSpaceKey, sampling.colorSpace);
    readEnum(import, kFilterKey, sampling.filter);
    readEnum(import, kWrapKey, sampling.wrap);
    if (const auto mipmaps = import.find(kMipmapsKey); mipmaps != import.end() && mipmaps->is_boolean()) {
        sampling.mipmaps = mipmaps->get<bool>();
    }

    return sampling;
}

nlohmann::json toJson(const TextureSampling& sampling)
{
    const TextureSampling defaults;
    nlohmann::json json = nlohmann::json::object();
    if (sampling.colorSpace != defaults.colorSpace) {
        json[kColorSpaceKey] = enumName(sampling.colorSpace);
    }
    if (sampling.filter != defaults.filter) {
        json[kFilterKey] = enumName(sampling.filter);
    }
    if (sampling.wrap != defaults.wrap) {
        json[kWrapKey] = enumName(sampling.wrap);
    }
    if (sampling.mipmaps != defaults.mipmaps) {
        json[kMipmapsKey] = sampling.mipmaps;
    }

    return json;
}

} // namespace yqlora
