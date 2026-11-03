#include "yqlora/editor/ui/panels/ResourceInspector.hpp"

#include "yqlora/editor/ui/core/EditorContext.hpp"
#include "yqlora/editor/ui/inspectors/MaterialInspector.hpp"
#include "yqlora/editor/ui/inspectors/ModelInspector.hpp"
#include "yqlora/editor/ui/inspectors/ShaderInspector.hpp"
#include "yqlora/editor/ui/inspectors/TextureInspector.hpp"
#include "yqlora/editor/ui/style/ResourceIcons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/material/MaterialData.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <imgui.h>

#include <string>

namespace yqlora::editor {

ResourceInspector::ResourceInspector()
{
    m_inspectors.emplace(ModelData::kResourceKind, std::make_unique<ModelInspector>());
    m_inspectors.emplace(TextureData::kResourceKind, std::make_unique<TextureInspector>());
    m_inspectors.emplace(MaterialData::kResourceKind, std::make_unique<MaterialInspector>());
    m_inspectors.emplace(ShaderData::kResourceKind, std::make_unique<ShaderInspector>());
}

ResourceInspector::~ResourceInspector() = default;

void ResourceInspector::draw(EditorContext& context, const ResourceInspection& inspection)
{
    const auto found = m_inspectors.find(inspection.kind);
    ResourceKindInspector* inspector = found != m_inspectors.end() ? found->second.get() : nullptr;
    if (inspection.id != m_id) {
        m_id = inspection.id;
        for (auto& [kind, each] : m_inspectors) {
            each->reset();
        }
    }
    ImGui::Text("%s  %s", resourceIcon(inspection.kind), toUtf8(inspection.path.filename()).c_str());
    const std::string kindName = inspection.kind.empty() ? tr("editor.resource.file")
                                                         : tr("editor.resourceKind." + std::string(inspection.kind));
    ImGui::TextDisabled("%s", kindName.c_str());
    ImGui::SetItemTooltip("%s", toGenericUtf8(inspection.path).c_str());

    if (inspection.status && inspection.status->state != ResourceState::Ready) {
        const char* key = inspection.status->state == ResourceState::Missing ? "editor.resource.missing"
                                                                             : "editor.resource.notLoaded";
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(
            toImVec4(context.theme.colors().errorText),
            "%s: %s",
            tr(key).c_str(),
            inspection.status->message.c_str()
        );
        ImGui::PopTextWrapPos();
    }
    if (inspector) {
        inspector->draw(context, inspection.id);
    }
}

} // namespace yqlora::editor
