#include "yqlora/editor/ui/widgets/DragDrop.hpp"

#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Node.hpp"

#include <imgui.h>

#include <cstdint>
#include <cstring>

namespace yqlora::editor {

namespace {

// Типы данных перетаскивания в ImGui, не длиннее 32 символов.
constexpr const char* kResourcePayload = "yqlora.resource";
constexpr const char* kNodePayload = "yqlora.node";
constexpr const char* kComponentPayload = "yqlora.component";

// Данные - массив 64-битных ID подряд.
void idsDragSource(const char* type, std::span<const std::uint64_t> ids, const std::string& label)
{
    if (ids.empty() || ids.front() == 0 || !ImGui::BeginDragDropSource()) {
        return;
    }
    ImGui::SetDragDropPayload(type, ids.data(), ids.size_bytes());
    ImGui::TextUnformatted(label.c_str());
    ImGui::EndDragDropSource();
}

std::vector<std::uint64_t> idsOf(const ImGuiPayload& payload)
{
    std::vector<std::uint64_t> ids(static_cast<std::size_t>(payload.DataSize) / sizeof(std::uint64_t));
    std::memcpy(ids.data(), payload.Data, ids.size() * sizeof(std::uint64_t));

    return ids;
}

// Тянут ли сейчас данные type и подходят ли они цели: неподходящие цель не
// подсвечивают, а курсор над ней показывает запрет.
template <typename Data>
bool refused(const char* type, const std::function<bool(const Data&)>& fits, Data (*read)(const ImGuiPayload&))
{
    const ImGuiPayload* dragged = ImGui::GetDragDropPayload();
    if (!dragged || !dragged->IsDataType(type) || fits(read(*dragged))) {
        return false;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
    }

    return true;
}

template <typename Data>
std::optional<Data> acceptDrop(
    const char* type,
    const std::function<bool(const Data&)>& fits,
    Data (*read)(const ImGuiPayload&)
)
{
    if ((fits && refused(type, fits, read)) || !ImGui::BeginDragDropTarget()) {
        return std::nullopt;
    }
    std::optional<Data> dropped;
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type)) {
        dropped = read(*payload);
    }
    ImGui::EndDragDropTarget();

    return dropped;
}

DraggedComponent componentOf(const ImGuiPayload& payload)
{
    DraggedComponent dragged;
    std::memcpy(&dragged, payload.Data, sizeof(dragged));

    return dragged;
}

template <typename Id>
std::optional<std::vector<Id>> acceptIds(const char* type, const std::function<bool(std::span<const Id>)>& fits)
{
    std::function<bool(const std::vector<std::uint64_t>&)> wrapped;
    if (fits) {
        wrapped = [&](const std::vector<std::uint64_t>& ids) {
            return fits(ids);
        };
    }

    return acceptDrop<std::vector<std::uint64_t>>(type, wrapped, idsOf);
}

} // namespace

void resourceDragSource(ResourceId id, const std::string& label)
{
    idsDragSource(kResourcePayload, std::span(&id, 1), label);
}

void resourceDragSource(std::span<const ResourceId> ids, const std::string& label)
{
    idsDragSource(kResourcePayload, ids, label);
}

std::optional<ResourceId> acceptResourceDrop()
{
    const auto ids = acceptIds<ResourceId>(kResourcePayload, {});

    return ids && !ids->empty() ? std::optional(ids->front()) : std::nullopt;
}

std::optional<std::vector<ResourceId>> acceptResourcesDrop(const std::function<bool(std::span<const ResourceId>)>& fits)
{
    return acceptIds<ResourceId>(kResourcePayload, fits);
}

void nodeDragSource(NodeId id, const std::string& label)
{
    idsDragSource(kNodePayload, std::span(&id, 1), label);
}

void nodeDragSource(std::span<const NodeId> ids, const std::string& label)
{
    idsDragSource(kNodePayload, ids, label);
}

std::optional<NodeId> acceptNodeDrop()
{
    const auto ids = acceptIds<NodeId>(kNodePayload, {});

    return ids && !ids->empty() ? std::optional(ids->front()) : std::nullopt;
}

std::optional<std::vector<NodeId>> acceptNodesDrop(const std::function<bool(std::span<const NodeId>)>& fits)
{
    return acceptIds<NodeId>(kNodePayload, fits);
}

void componentDragSource(const Component& component, const std::string& label)
{
    if (!component.owner() || !ImGui::BeginDragDropSource()) {
        return;
    }
    const DraggedComponent dragged{component.owner()->id(), component.id()};
    ImGui::SetDragDropPayload(kComponentPayload, &dragged, sizeof(dragged));
    ImGui::TextUnformatted(label.c_str());
    ImGui::EndDragDropSource();
}

std::optional<DraggedComponent> acceptComponentDrop(const std::function<bool(const DraggedComponent&)>& fits)
{
    return acceptDrop<DraggedComponent>(kComponentPayload, fits, componentOf);
}

} // namespace yqlora::editor
