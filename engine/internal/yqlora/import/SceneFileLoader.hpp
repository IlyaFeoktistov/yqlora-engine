#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceLoader.hpp"

#include <array>
#include <span>
#include <string_view>

namespace yqlora {

// Файлы сцен (.yscene) и префабов (.yprefab) в SceneData и PrefabData: один
// формат и один разбор, виды разные - уровень не ставится туда, где ждут
// префаб. Узлы здесь не строятся: это дело того, кто ставит префаб в сцену.
class YQLORA_API SceneFileLoader final : public ResourceLoader {
public:
    // kind - kinds::kScene или kinds::kPrefab.
    SceneFileLoader(std::string_view extension, std::string_view kind);

    std::span<const std::string_view> extensions() const override;
    std::string_view kind() const override;
    LoadResult load(const LoadRequest& request) const override;

private:
    std::array<std::string_view, 1> m_extensions;
    std::string_view m_kind;
};

} // namespace yqlora
