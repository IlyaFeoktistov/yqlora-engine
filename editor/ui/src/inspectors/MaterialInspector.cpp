#include "yqlora/editor/ui/inspectors/MaterialInspector.hpp"

#include "yqlora/editor/ui/core/EditorContext.hpp"
#include "yqlora/editor/ui/core/EditorHost.hpp"
#include "yqlora/editor/ui/widgets/EnumCombo.hpp"
#include "yqlora/editor/ui/widgets/PropertyContext.hpp"
#include "yqlora/editor/ui/widgets/ResourceField.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/material/MaterialData.hpp"
#include "yqlora/render/RenderTarget.hpp"
#include "yqlora/render/Renderer.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceKinds.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

// Превью - квадрат шириной в панель, но не больше этого размера в пикселях.
constexpr int kMaxPreviewSize = 256;
constexpr float kDragSpeed = 0.01f;
constexpr int kMaxComponents = 4;

std::string fieldLabel(const std::string& label, const std::string& name)
{
    return (label.empty() ? name : tr(label)) + "##" + name;
}

// Виджет поля по типу и пометкам; true, если значение поменяли.
bool fieldWidget(const MaterialField& field, std::vector<float>& value)
{
    const std::string label = fieldLabel(field.label, field.name);
    const int count = static_cast<int>(value.size());
    if (field.color && count == 4) {
        return ImGui::ColorEdit4(label.c_str(), value.data());
    }
    if (field.color && count == 3) {
        return ImGui::ColorEdit3(label.c_str(), value.data());
    }
    const bool integer = field.type == rhi::ValueType::Int || field.type == rhi::ValueType::UInt;
    if (integer && count == 1) {
        int number = static_cast<int>(value[0]);
        const bool changed = field.min && field.max ? ImGui::SliderInt(
                                                          label.c_str(),
                                                          &number,
                                                          static_cast<int>(*field.min),
                                                          static_cast<int>(*field.max)
                                                      )
                                                    : ImGui::DragInt(label.c_str(), &number);
        value[0] = static_cast<float>(number);

        return changed;
    }
    if (count == 1 && field.min && field.max) {
        return ImGui::SliderFloat(label.c_str(), value.data(), *field.min, *field.max);
    }
    if (count < 1 || count > kMaxComponents) {
        return false;
    }

    return ImGui::DragScalarN(label.c_str(), ImGuiDataType_Float, value.data(), count, kDragSpeed);
}

} // namespace

MaterialInspector::MaterialInspector() = default;

MaterialInspector::~MaterialInspector() = default;

void MaterialInspector::reset()
{
}

void MaterialInspector::draw(EditorContext& context, ResourceId id)
{
    MaterialEditing& editing = context.session.materialEditing();
    const MaterialData* material = editing.material(id);
    if (!material) {
        return;
    }
    drawPreview(context, id);
    // Копия: правка ставит в кэш новые данные, прежние указатели устаревают.
    const MaterialData current = *material;
    drawState(context, id, current);
    const ShaderData* shader = context.session.resources().find<ShaderData>(current.shader);
    if (!shader) {
        ImGui::TextDisabled("%s", tr("editor.material.noShader").c_str());

        return;
    }
    drawFields(context, id, current, *shader);
    drawTextures(context, id, current, *shader);
}

void MaterialInspector::drawPreview(EditorContext& context, ResourceId id)
{
    if (!m_preview) {
        m_preview.emplace(context.session.services());
    }
    const int size = std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x), 1, kMaxPreviewSize);
    Renderer& renderer = context.host.renderer();
    if (!m_target) {
        m_target = renderer.createRenderTarget(size, size);
    }
    m_target->resize(size, size);
    renderer.render(m_preview->frame(id, 1.0f), *m_target);
    const UiTexture texture = m_target->uiTexture();
    ImGui::Image(
        static_cast<ImTextureID>(texture.id),
        ImVec2(static_cast<float>(size), static_cast<float>(size)),
        ImVec2(texture.uv0.x, texture.uv0.y),
        ImVec2(texture.uv1.x, texture.uv1.y)
    );
}

void MaterialInspector::drawState(EditorContext& context, ResourceId id, const MaterialData& material)
{
    MaterialEditing& editing = context.session.materialEditing();
    const PropertyEditorContext fields = propertyEditorContext(context);
    ResourceRef shader{material.shader};
    ImGui::TextUnformatted(tr("editor.material.shader").c_str());
    if (resourceField(kinds::kShader, shader, fields)) {
        editing.change(id, [&](MaterialData& edited) {
            edited.shader = shader.id;
        });
        editing.finish();
    }
    MaterialBlend blend = material.blend;
    if (enumCombo(tr("editor.material.blend").c_str(), blend)) {
        editing.change(id, [&](MaterialData& edited) {
            edited.blend = blend;
        });
        editing.finish();
    }
    MaterialCull cull = material.cull;
    if (enumCombo(tr("editor.material.cull").c_str(), cull)) {
        editing.change(id, [&](MaterialData& edited) {
            edited.cull = cull;
        });
        editing.finish();
    }
}

void MaterialInspector::drawFields(
    EditorContext& context,
    ResourceId id,
    const MaterialData& material,
    const ShaderData& shader
)
{
    MaterialEditing& editing = context.session.materialEditing();
    ImGui::SeparatorText(tr("editor.material.values").c_str());
    for (const MaterialField& field : shader.fields) {
        if (field.hidden) {
            continue;
        }
        const std::span<const float> stored = materialValue(material, field);
        std::vector<float> value(stored.begin(), stored.end());
        if (fieldWidget(field, value)) {
            editing.change(id, [&](MaterialData& edited) {
                edited.values[field.name] = value;
            });
        }
        // Ползунок отпустили, значение ввели: правка закончена.
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            editing.finish();
        }
    }
}

void MaterialInspector::drawTextures(
    EditorContext& context,
    ResourceId id,
    const MaterialData& material,
    const ShaderData& shader
)
{
    MaterialEditing& editing = context.session.materialEditing();
    const PropertyEditorContext fields = propertyEditorContext(context);
    if (std::ranges::all_of(shader.textures, &MaterialTexture::hidden)) {
        return;
    }
    ImGui::SeparatorText(tr("editor.material.textures").c_str());
    for (const MaterialTexture& texture : shader.textures) {
        if (texture.hidden) {
            continue;
        }
        ImGui::PushID(texture.name.c_str());
        ImGui::TextUnformatted((texture.label.empty() ? texture.name : tr(texture.label)).c_str());
        ResourceRef value{materialTexture(material, texture)};
        if (resourceField(kinds::kTexture, value, fields)) {
            editing.change(id, [&](MaterialData& edited) {
                if (value.id == 0) {
                    edited.textures.erase(texture.name);
                } else {
                    edited.textures[texture.name] = value.id;
                }
            });
            editing.finish();
        }
        ImGui::PopID();
    }
}

} // namespace yqlora::editor
