#include "yqlora/editor/ui/inspectors/PhysicsMaterialInspector.hpp"

#include "yqlora/editor/ui/core/EditorContext.hpp"
#include "yqlora/editor/ui/widgets/PropertyContext.hpp"
#include "yqlora/editor/ui/widgets/PropertyTable.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/physics/PhysicsMaterialData.hpp"

#include <imgui.h>

namespace yqlora::editor {

void PhysicsMaterialInspector::reset()
{
}

void PhysicsMaterialInspector::draw(EditorContext& context, ResourceId id)
{
    ResourceEditing& editing = context.session.resourceEditing();
    const PhysicsMaterialData* material = editing.find<PhysicsMaterialData>(id);
    if (!material) {
        return;
    }
    const Property& record = recordProperty<PhysicsMaterialData>();
    const RecordValue value = PropertyStorage<PhysicsMaterialData>::store(*material);
    ImGui::BeginDisabled(!editing.editable(id));
    const PropertyResult result = drawRecordTable(propertyEditorContext(context), record, value);
    ImGui::EndDisabled();
    if (result.changed) {
        const auto edited = PropertyStorage<PhysicsMaterialData>::load(std::get<RecordValue>(result.value));
        editing.change<PhysicsMaterialData>(id, [&](PhysicsMaterialData& data) {
            data = edited;
        });
    }
    // Ползунок отпустили, значение ввели или выбрали: правка закончена.
    if (editing.pending() && !ImGui::IsAnyItemActive()) {
        editing.finish();
    }
}

} // namespace yqlora::editor
