#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/Light3D.hpp"

namespace yqlora {

// Свет издалека, одинаковый во всей сцене, как солнце: важен только поворот узла.
class YQLORA_API DirectionalLight3D final : public Light3D {
public:
    explicit DirectionalLight3D(std::string name = "DirectionalLight3D");

    YQLORA_TYPE(DirectionalLight3D, Light3D)
};

} // namespace yqlora
