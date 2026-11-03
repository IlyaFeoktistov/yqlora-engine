#include "yqlora/editor/ui/style/NodeIcons.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"

#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/DirectionalLight3D.hpp"
#include "yqlora/render/PointLight3D.hpp"
#include "yqlora/render/SpotLight3D.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace yqlora::editor {

namespace {

const std::pair<std::string_view, const char*> kIcons[] = {
    {Node3D::staticTypeName(), icons::kNode3D},
    {Camera3D::staticTypeName(), icons::kCamera3D},
    {DirectionalLight3D::staticTypeName(), icons::kDirectionalLight3D},
    {PointLight3D::staticTypeName(), icons::kPointLight3D},
    {SpotLight3D::staticTypeName(), icons::kSpotLight3D},
};

} // namespace

const char* nodeIcon(std::string_view type)
{
    const auto found = std::ranges::find(kIcons, type, &std::pair<std::string_view, const char*>::first);

    return found != std::end(kIcons) ? found->second : icons::kNode;
}

} // namespace yqlora::editor
