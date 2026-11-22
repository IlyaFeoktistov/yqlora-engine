#include "yqlora/editor/ui/widgets/DebugChannelsMenu.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/widgets/IconButton.hpp"

#include "yqlora/editor/EditorSettings.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/scene/Scene.hpp"

#include <imgui.h>

#include <string>
#include <string_view>

namespace yqlora::editor {

namespace {

constexpr std::string_view kChannelKeyPrefix = "editor.debug.channel.";

// Каналы подсистем - переведёнными названиями, свои каналы игры - как их назвали.
std::string channelLabel(const std::string& channel)
{
    return trOr(std::string(kChannelKeyPrefix) + channel, channel);
}

} // namespace

bool debugChannelsMenu(const char* id, const Scene& scene, DebugChannels& shown, EditorSettings& settings)
{
    ImGui::PushID(id);
    if (iconButton(icons::kDebugGizmos, !shown.empty())) {
        ImGui::OpenPopup("channels");
    }
    const bool hovered = ImGui::IsItemHovered();
    ImGui::SetItemTooltip("%s", tr("editor.debug.gizmos").c_str());

    if (ImGui::BeginPopup("channels")) {
        ImGui::SeparatorText(tr("editor.debug.gizmos").c_str());
        DebugChannels channels = scene.debugChannels();
        channels.insert(shown.begin(), shown.end());
        for (const std::string& channel : channels) {
            bool on = shown.contains(channel);
            const std::string label = channelLabel(channel) + "##" + channel;
            if (ImGui::Checkbox(label.c_str(), &on)) {
                if (on) {
                    shown.insert(channel);
                } else {
                    shown.erase(channel);
                }
                settings.save();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    return hovered;
}

} // namespace yqlora::editor
