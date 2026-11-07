#include "yqlora/import/GltfMaterials.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/import/GltfImages.hpp"
#include "yqlora/import/ImageLoader.hpp"
#include "yqlora/import/OrmPacking.hpp"
#include "yqlora/material/MaterialData.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/render/TextureData.hpp"

#include <cgltf.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace yqlora {

namespace {

namespace fs = std::filesystem;

// Поля и текстуры шейдера Standard, которые заполняет материал glTF.
constexpr const char* kBaseColorField = "baseColor";
constexpr const char* kMetallicField = "metallic";
constexpr const char* kRoughnessField = "roughness";
constexpr const char* kOcclusionStrengthField = "occlusionStrength";
constexpr const char* kNormalScaleField = "normalScale";
constexpr const char* kEmissionField = "emission";
constexpr const char* kEmissionIntensityField = "emissionIntensity";
constexpr const char* kEmissionExposureWeightField = "emissionExposureWeight";
constexpr const char* kBaseColorMap = "baseColorMap";
constexpr const char* kOrmMap = "ormMap";
constexpr const char* kNormalMap = "normalMap";
constexpr const char* kEmissionMap = "emissionMap";

// Свечение glTF без единиц: 1 - белый на экране при любой экспозиции.
constexpr float kScreenEmission = 0.0f;
// Картинка, нужная и в sRGB, и линейной: у линейной копии к имени дописано это.
constexpr const char* kLinearSuffix = " linear";
constexpr const char* kOrmSuffix = " ORM";
constexpr const char* kOrmName = "ORM";

using ImageIndex = std::size_t;
using TextureUse = std::pair<ImageIndex, ColorSpace>;

// Картинка вида текстуры; nullopt - вида нет или его картинка в формате,
// которого движок не читает (KTX2, WebP).
std::optional<ImageIndex> imageOf(const cgltf_data& data, const cgltf_texture_view& view)
{
    if (!view.texture || !view.texture->image) {
        return std::nullopt;
    }

    return cgltf_image_index(&data, view.texture->image);
}

TextureSampling samplingOf(const cgltf_texture_view& view, ColorSpace colorSpace)
{
    TextureSampling sampling;
    sampling.colorSpace = colorSpace;
    const cgltf_sampler* sampler = view.texture ? view.texture->sampler : nullptr;
    if (!sampler) {
        return sampling;
    }
    // Повтор у нас один на обе оси; по U, как чаще задают.
    switch (sampler->wrap_s) {
    case cgltf_wrap_mode_clamp_to_edge: sampling.wrap = TextureWrap::Clamp; break;
    case cgltf_wrap_mode_mirrored_repeat: sampling.wrap = TextureWrap::Mirror; break;
    case cgltf_wrap_mode_repeat: sampling.wrap = TextureWrap::Repeat; break;
    }
    if (sampler->mag_filter == cgltf_filter_type_nearest) {
        sampling.filter = TextureFilter::Nearest;
    }
    if (sampler->min_filter == cgltf_filter_type_nearest || sampler->min_filter == cgltf_filter_type_linear) {
        sampling.mipmaps = false;
    }

    return sampling;
}

// Откуда материал берёт ormMap.
struct OrmSource {
    std::optional<ImageIndex> occlusion;
    std::optional<ImageIndex> metallicRoughness;

    // Одна картинка на всё или только metallicRoughness: берётся как есть.
    bool direct() const
    {
        return metallicRoughness && (!occlusion || occlusion == metallicRoughness);
    }

    bool any() const
    {
        return occlusion || metallicRoughness;
    }
};

OrmSource ormSourceOf(const cgltf_data& data, const cgltf_material& material)
{
    return {
        .occlusion = imageOf(data, material.occlusion_texture),
        .metallicRoughness = imageOf(data, material.pbr_metallic_roughness.metallic_roughness_texture),
    };
}

class Reader {
public:
    Reader(const cgltf_data& data, const fs::path& file, SubresourceKeys& keys)
        : m_data(data)
        , m_file(file)
        , m_fileName(toUtf8(file.filename()))
        , m_keys(keys)
    {
    }

    GltfMaterials read()
    {
        collectUses();
        addTextures();
        addPackedOrm();
        for (cgltf_size m = 0; m < m_data.materials_count; ++m) {
            addMaterial(m);
        }

        return std::move(m_result);
    }

private:
    void use(const cgltf_texture_view& view, ColorSpace colorSpace)
    {
        if (view.texture && !view.texture->image) {
            log::warn("glTF '{}': a texture has no image in a supported format (PNG, JPEG), skipped", m_fileName);
        }
        if (const auto image = imageOf(m_data, view)) {
            m_uses.try_emplace({*image, colorSpace}, samplingOf(view, colorSpace));
        }
    }

    void collectUses()
    {
        for (cgltf_size m = 0; m < m_data.materials_count; ++m) {
            const cgltf_material& material = m_data.materials[m];
            use(material.pbr_metallic_roughness.base_color_texture, ColorSpace::Srgb);
            use(material.emissive_texture, ColorSpace::Srgb);
            use(material.normal_texture, ColorSpace::Linear);
            const OrmSource orm = ormSourceOf(m_data, material);
            if (orm.direct()) {
                use(material.pbr_metallic_roughness.metallic_roughness_texture, ColorSpace::Linear);
            }
        }
    }

    // Картинка файла, прочитанная один раз; nullptr - не прочиталась.
    const TextureData* decoded(ImageIndex index)
    {
        auto [it, added] = m_decoded.try_emplace(index);
        if (added) {
            const cgltf_image& image = m_data.images[index];
            std::string error;
            const auto bytes = readGltfImageBytes(image, m_file, m_result.dependencies, error);
            if (bytes) {
                it->second = decodeImage(*bytes, TextureSampling{}, error);
            }
            if (!it->second) {
                log::warn("glTF '{}': image {} is not read: {}", m_fileName, index, error);
            }
        }

        return it->second ? &*it->second : nullptr;
    }

    void addTextures()
    {
        for (const auto& [use, sampling] : m_uses) {
            const auto [index, colorSpace] = use;
            const TextureData* image = decoded(index);
            if (!image) {
                continue;
            }
            std::string name = gltfImageName(m_data.images[index]);
            const bool twin = colorSpace == ColorSpace::Linear && m_uses.contains({index, ColorSpace::Srgb});
            if (twin) {
                name += kLinearSuffix;
            }
            TextureData texture = *image;
            texture.sampling = sampling;
            // У HDR цвета уже линейные.
            if (texture.format != PixelFormat::Rgba8) {
                texture.sampling.colorSpace = ColorSpace::Linear;
            }
            std::string key = m_keys.next(TextureData::kResourceKind, name, index);
            m_textureKeys.emplace(use, key);
            m_result.resources.push_back(makeLoadedResource(std::move(key), std::move(texture)));
        }
    }

    void addPackedOrm()
    {
        m_ormKeys.resize(m_data.materials_count);
        for (cgltf_size m = 0; m < m_data.materials_count; ++m) {
            const cgltf_material& material = m_data.materials[m];
            const OrmSource orm = ormSourceOf(m_data, material);
            if (!orm.any() || orm.direct()) {
                continue;
            }
            const TextureData* occlusion = orm.occlusion ? decoded(*orm.occlusion) : nullptr;
            const TextureData* metallicRoughness = orm.metallicRoughness ? decoded(*orm.metallicRoughness) : nullptr;
            auto packed = packOrm(occlusion, metallicRoughness);
            if (!packed) {
                log::warn(
                    "glTF '{}': occlusion and metallic-roughness of material {} are not packed into one map (not "
                    "8-bit images), occlusion is ignored",
                    m_fileName,
                    m
                );
                continue;
            }
            if (!occlusion || !metallicRoughness) {
                packed->sampling = samplingOf(
                    occlusion ? material.pbr_metallic_roughness.metallic_roughness_texture : material.occlusion_texture,
                    ColorSpace::Linear
                );
            }
            const std::string name = material.name && *material.name ? std::string(material.name) + kOrmSuffix
                                                                     : std::string(kOrmName);
            std::string key = m_keys.next(TextureData::kResourceKind, name, m);
            m_ormKeys[m] = key;
            m_result.resources.push_back(makeLoadedResource(std::move(key), std::move(*packed)));
        }
    }

    // Ключ текстуры вида view в цветовом пространстве colorSpace; nullopt -
    // текстуры нет (вида нет, картинка не прочиталась).
    std::optional<std::string> textureKey(const cgltf_texture_view& view, ColorSpace colorSpace) const
    {
        const auto image = imageOf(m_data, view);
        const auto found = image ? m_textureKeys.find({*image, colorSpace}) : m_textureKeys.end();

        return found != m_textureKeys.end() ? std::optional(found->second) : std::nullopt;
    }

    void addMaterial(cgltf_size index)
    {
        const cgltf_material& source = m_data.materials[index];
        auto material = std::make_shared<MaterialData>();
        LoadedResource resource{
            .key = m_keys.next(MaterialData::kResourceKind, source.name ? source.name : "", index),
            .kind = MaterialData::kResourceKind,
            .data = material,
        };
        const auto link = [&](const char* field, const std::optional<std::string>& key) {
            if (key) {
                resource.links.push_back({*key, [material, field](ResourceId id) {
                                              material->textures[field] = id;
                                          }});
            }
        };
        const auto set = [&](const char* field, std::vector<float> values) {
            material->values[field] = std::move(values);
        };

        const cgltf_pbr_metallic_roughness& pbr = source.pbr_metallic_roughness;
        const Vec3 base(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]);
        set(kBaseColorField,
            {math::linearToSrgb(base.r),
             math::linearToSrgb(base.g),
             math::linearToSrgb(base.b),
             pbr.base_color_factor[3]});
        set(kMetallicField, {pbr.metallic_factor});
        set(kRoughnessField, {pbr.roughness_factor});
        link(kBaseColorMap, textureKey(pbr.base_color_texture, ColorSpace::Srgb));

        const OrmSource orm = ormSourceOf(m_data, source);
        if (orm.direct()) {
            link(kOrmMap, textureKey(pbr.metallic_roughness_texture, ColorSpace::Linear));
        } else if (m_ormKeys[index]) {
            link(kOrmMap, m_ormKeys[index]);
        }
        const bool occlusionPacked = orm.occlusion && (orm.direct() || m_ormKeys[index]);
        if (occlusionPacked) {
            set(kOcclusionStrengthField, {source.occlusion_texture.scale});
        } else if (orm.metallicRoughness) {
            // В R картинки metallicRoughness без затенения может лежать что угодно.
            set(kOcclusionStrengthField, {0.0f});
        }

        if (source.normal_texture.texture) {
            set(kNormalScaleField, {source.normal_texture.scale});
            link(kNormalMap, textureKey(source.normal_texture, ColorSpace::Linear));
        }

        const Vec3 emissive(source.emissive_factor[0], source.emissive_factor[1], source.emissive_factor[2]);
        const float strength = source.has_emissive_strength ? source.emissive_strength.emissive_strength : 1.0f;
        const float largest = std::max({emissive.r, emissive.g, emissive.b});
        if (largest > 0.0f) {
            const Vec3 color = emissive / largest;
            set(kEmissionField,
                {math::linearToSrgb(color.r), math::linearToSrgb(color.g), math::linearToSrgb(color.b)});
            set(kEmissionIntensityField, {largest * strength});
            set(kEmissionExposureWeightField, {kScreenEmission});
            link(kEmissionMap, textureKey(source.emissive_texture, ColorSpace::Srgb));
        }

        switch (source.alpha_mode) {
        case cgltf_alpha_mode_opaque: break;
        case cgltf_alpha_mode_mask: set(kAlphaCutoffField.data(), {source.alpha_cutoff}); break;
        case cgltf_alpha_mode_blend: material->blend = MaterialBlend::Transparent; break;
        case cgltf_alpha_mode_max_enum: break;
        }
        if (source.double_sided) {
            material->cull = MaterialCull::None;
        }
        warnSecondUv(source, index);

        m_result.materialKeys.push_back(resource.key);
        m_result.resources.push_back(std::move(resource));
    }

    void warnSecondUv(const cgltf_material& material, cgltf_size index) const
    {
        const cgltf_texture_view* views[] = {
            &material.pbr_metallic_roughness.base_color_texture,
            &material.pbr_metallic_roughness.metallic_roughness_texture,
            &material.normal_texture,
            &material.occlusion_texture,
            &material.emissive_texture,
        };
        if (std::ranges::any_of(views, [](const cgltf_texture_view* view) {
                return view->texture && view->texcoord != 0;
            })) {
            log::warn(
                "glTF '{}': material {} uses a second set of texture coordinates, the first one (uv0) is used",
                m_fileName,
                index
            );
        }
    }

    const cgltf_data& m_data;
    const fs::path& m_file;
    std::string m_fileName;
    SubresourceKeys& m_keys;
    GltfMaterials m_result;
    // Что из картинок нужно и в каком виде, с сэмплингом первого
    // использования; упорядочено по картинке, sRGB раньше линейной.
    std::map<TextureUse, TextureSampling> m_uses;
    std::map<ImageIndex, std::optional<TextureData>> m_decoded;
    std::map<TextureUse, std::string> m_textureKeys;
    std::vector<std::optional<std::string>> m_ormKeys;
};

} // namespace

GltfMaterials readGltfMaterials(const cgltf_data& data, const fs::path& file, SubresourceKeys& keys)
{
    return Reader(data, file, keys).read();
}

} // namespace yqlora
