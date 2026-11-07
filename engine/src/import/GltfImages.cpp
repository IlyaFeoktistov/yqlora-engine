#include "yqlora/import/GltfImages.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/platform/Files.hpp"

#include <cgltf.h>

#include <cstdlib>
#include <string_view>

namespace yqlora {

namespace {

constexpr std::string_view kDataUriPrefix = "data:";
constexpr std::string_view kBase64Marker = ";base64,";
constexpr char kBase64Padding = '=';
// Четыре знака base64 дают три байта.
constexpr std::size_t kBase64Chars = 4;
constexpr std::size_t kBase64Bytes = 3;

std::string decodedUri(const char* uri)
{
    std::string decoded(uri);
    decoded.resize(cgltf_decode_uri(decoded.data()));

    return decoded;
}

std::optional<std::string> fromDataUri(std::string_view uri, std::string& error)
{
    const std::size_t marker = uri.find(kBase64Marker);
    if (marker == std::string_view::npos) {
        error = "a data URI of an image is not base64";

        return std::nullopt;
    }
    const std::string payload(uri.substr(marker + kBase64Marker.size()));
    std::size_t padding = 0;
    for (auto it = payload.rbegin(); it != payload.rend() && *it == kBase64Padding; ++it) {
        ++padding;
    }
    if (payload.size() % kBase64Chars != 0 || padding > kBase64Chars) {
        error = "a data URI of an image is broken";

        return std::nullopt;
    }
    const std::size_t size = payload.size() / kBase64Chars * kBase64Bytes - padding;
    cgltf_options options{};
    void* decoded = nullptr;
    if (size == 0 || cgltf_load_buffer_base64(&options, size, payload.c_str(), &decoded) != cgltf_result_success) {
        error = "a data URI of an image is broken";

        return std::nullopt;
    }
    std::string bytes(static_cast<const char*>(decoded), size);
    std::free(decoded);

    return bytes;
}

} // namespace

std::optional<std::string> readGltfImageBytes(
    const cgltf_image& image,
    const std::filesystem::path& file,
    std::vector<std::filesystem::path>& dependencies,
    std::string& error
)
{
    if (image.buffer_view) {
        const std::uint8_t* data = cgltf_buffer_view_data(image.buffer_view);
        if (!data) {
            error = "the buffer of an image is not loaded";

            return std::nullopt;
        }

        return std::string(reinterpret_cast<const char*>(data), image.buffer_view->size);
    }
    if (!image.uri) {
        error = "an image has neither a buffer nor a URI";

        return std::nullopt;
    }
    if (std::string_view(image.uri).starts_with(kDataUriPrefix)) {
        return fromDataUri(image.uri, error);
    }
    const std::filesystem::path path = file.parent_path() / fromUtf8(decodedUri(image.uri));
    dependencies.push_back(path);
    auto bytes = platform::readFile(path);
    if (!bytes || bytes->empty()) {
        error = "the image file '" + decodedUri(image.uri) + "' is missing or empty";

        return std::nullopt;
    }

    return bytes;
}

std::string gltfImageName(const cgltf_image& image)
{
    if (image.name && *image.name) {
        return image.name;
    }
    if (image.uri && !std::string_view(image.uri).starts_with(kDataUriPrefix)) {
        return toUtf8(fromUtf8(decodedUri(image.uri)).stem());
    }

    return {};
}

} // namespace yqlora
