#pragma once

// Сборка файлов glTF в тестах: двоичный буфер, виды и аксессоры собираются
// кодом, остальное - JSON руками. Так каждый случай виден в самом тесте.

#include <yqlora/core/Utf8Path.hpp>
#include <yqlora/math/Math.hpp>

#include <glm/common.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace yqlora::test {

// Коды glTF.
inline constexpr int kFloat = 5126;
inline constexpr int kUnsignedShort = 5123;
inline constexpr int kTriangles = 4;
inline constexpr int kPoints = 0;
inline constexpr std::uint32_t kGlbMagic = 0x46546C67;
inline constexpr std::uint32_t kGlbVersion = 2;
inline constexpr std::uint32_t kJsonChunk = 0x4E4F534A;
inline constexpr std::uint32_t kBinChunk = 0x004E4942;

inline std::string base64(const std::vector<std::uint8_t>& bytes)
{
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::uint32_t chunk = (bytes[i] << 16) | (i + 1 < bytes.size() ? bytes[i + 1] << 8 : 0)
                                    | (i + 2 < bytes.size() ? bytes[i + 2] : 0);
        text += kAlphabet[(chunk >> 18) & 63];
        text += kAlphabet[(chunk >> 12) & 63];
        text += i + 1 < bytes.size() ? kAlphabet[(chunk >> 6) & 63] : '=';
        text += i + 2 < bytes.size() ? kAlphabet[chunk & 63] : '=';
    }

    return text;
}

using Rgba = std::array<std::uint8_t, 4>;

// Картинка TGA без сжатия, строки сверху вниз: её читает stb_image, а
// собрать её можно без библиотек.
inline std::vector<std::uint8_t> tga(int width, int height, const std::vector<Rgba>& pixels)
{
    constexpr std::uint8_t kUncompressedTrueColor = 2;
    constexpr std::uint8_t kBitsPerPixel = 32;
    // 8 бит альфы и начало в левом верхнем углу.
    constexpr std::uint8_t kDescriptor = 0x28;
    std::vector<std::uint8_t> bytes{0, 0, kUncompressedTrueColor, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto word = [&](int value) {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    };
    word(width);
    word(height);
    bytes.push_back(kBitsPerPixel);
    bytes.push_back(kDescriptor);
    for (const Rgba& pixel : pixels) {
        bytes.insert(bytes.end(), {pixel[2], pixel[1], pixel[0], pixel[3]});
    }

    return bytes;
}

// Собирает glTF: двоичный буфер, виды и аксессоры, остальное - JSON руками.
struct GltfBuilder {
    template <typename T>
    int view(const std::vector<T>& values, int stride = 0)
    {
        while (bin.size() % 4 != 0) {
            bin.push_back(0);
        }
        const std::size_t offset = bin.size();
        bin.resize(offset + values.size() * sizeof(T));
        std::memcpy(bin.data() + offset, values.data(), values.size() * sizeof(T));
        nlohmann::json entry{{"buffer", 0}, {"byteOffset", offset}, {"byteLength", values.size() * sizeof(T)}};
        if (stride != 0) {
            entry["byteStride"] = stride;
        }
        json["bufferViews"].push_back(entry);

        return static_cast<int>(json["bufferViews"].size()) - 1;
    }

    int accessor(int bufferView, int offset, int componentType, int count, const char* type, nlohmann::json extra = {})
    {
        nlohmann::json entry{
            {"bufferView", bufferView},
            {"byteOffset", offset},
            {"componentType", componentType},
            {"count", count},
            {"type", type}
        };
        if (extra.is_object()) {
            entry.update(extra);
        }
        json["accessors"].push_back(entry);

        return static_cast<int>(json["accessors"].size()) - 1;
    }

    // Картинка в двоичном буфере и текстура с ней; индекс текстуры.
    int texture(const std::vector<std::uint8_t>& bytes, const char* name = nullptr, nlohmann::json sampler = {})
    {
        nlohmann::json image{{"bufferView", view(bytes)}, {"mimeType", "image/png"}};
        if (name) {
            image["name"] = name;
        }
        json["images"].push_back(image);
        nlohmann::json entry{{"source", json["images"].size() - 1}};
        if (sampler.is_object()) {
            json["samplers"].push_back(sampler);
            entry["sampler"] = json["samplers"].size() - 1;
        }
        json["textures"].push_back(entry);

        return static_cast<int>(json["textures"].size()) - 1;
    }

    // Аксессор VEC3 из чисел подряд, с границами: их требует glTF для положений.
    int positions(const std::vector<float>& values)
    {
        Vec3 low(values[0], values[1], values[2]);
        Vec3 high = low;
        for (std::size_t i = 0; i < values.size(); i += 3) {
            low = glm::min(low, Vec3(values[i], values[i + 1], values[i + 2]));
            high = glm::max(high, Vec3(values[i], values[i + 1], values[i + 2]));
        }
        const int count = static_cast<int>(values.size() / 3);

        return accessor(
            view(values),
            0,
            kFloat,
            count,
            "VEC3",
            {{"min", {low.x, low.y, low.z}}, {"max", {high.x, high.y, high.z}}}
        );
    }

    nlohmann::json document(nlohmann::json buffer) const
    {
        nlohmann::json result = json;
        result["asset"] = {{"version", "2.0"}};
        buffer["byteLength"] = bin.size();
        result["buffers"] = nlohmann::json::array({buffer});

        return result;
    }

    void writeGltf(const std::filesystem::path& file) const
    {
        std::ofstream(file) << document({{"uri", "data:application/octet-stream;base64," + base64(bin)}}).dump();
    }

    // .gltf и рядом внешний .bin.
    void writeGltfWithBin(const std::filesystem::path& file, const std::string& binName) const
    {
        std::ofstream(file.parent_path() / fromUtf8(binName), std::ios::binary)
            .write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
        std::ofstream(file) << document({{"uri", binName}}).dump();
    }

    void writeGlb(const std::filesystem::path& file) const
    {
        std::string text = document(nlohmann::json::object()).dump();
        while (text.size() % 4 != 0) {
            text += ' ';
        }
        std::vector<std::uint8_t> data = bin;
        while (data.size() % 4 != 0) {
            data.push_back(0);
        }
        const auto word = [](std::ofstream& out, std::uint32_t value) {
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        };
        std::ofstream out(file, std::ios::binary);
        word(out, kGlbMagic);
        word(out, kGlbVersion);
        word(out, static_cast<std::uint32_t>(12 + 8 + text.size() + 8 + data.size()));
        word(out, static_cast<std::uint32_t>(text.size()));
        word(out, kJsonChunk);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        word(out, static_cast<std::uint32_t>(data.size()));
        word(out, kBinChunk);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }

    nlohmann::json json = nlohmann::json::object();
    std::vector<std::uint8_t> bin;
};

} // namespace yqlora::test
