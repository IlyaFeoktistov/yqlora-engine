# Логика редактора без интерфейса: проекты, сессия, история правок, выделение,
# камера, выбор объектов. ImGui сюда не подключается, поэтому интерфейс в логику не попадёт.
# Шейдеры редактора (сетка, контур выделения) вшиваются в его библиотеку,
# как шейдеры движка в свою.
set(YQLORA_EDITOR_SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/background.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/background.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline_mask.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline_mask.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline.frag
)
set(YQLORA_EDITOR_SHADERS_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedEditorShaders.hpp)
string(REPLACE ";" "|" YQLORA_EDITOR_SHADERS_ARG "${YQLORA_EDITOR_SHADERS}")
add_custom_command(
    OUTPUT ${YQLORA_EDITOR_SHADERS_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${YQLORA_EDITOR_SHADERS_HEADER}
        -DROOT=${CMAKE_CURRENT_SOURCE_DIR}/shaders
        -DNAMESPACE=yqlora::editor::shaders
        -DSHADERS=${YQLORA_EDITOR_SHADERS_ARG}
        -P ${PROJECT_SOURCE_DIR}/cmake/EmbedShaders.cmake
    DEPENDS ${YQLORA_EDITOR_SHADERS} ${PROJECT_SOURCE_DIR}/cmake/EmbedShaders.cmake
    COMMENT "Embedding editor shaders"
    VERBATIM
)

add_library(yqlora_editor STATIC
    ${YQLORA_EDITOR_SHADERS_HEADER}
    src/EditorCamera.cpp
    src/Picking.cpp
    src/PropertyValues.cpp
    src/EditorSession.cpp
    src/EditorSettings.cpp
    src/GameLibrary.cpp
    src/Inspection.cpp
    src/LayerUsage.cpp
    src/MaterialEditing.cpp
    src/MaterialPreview.cpp
    src/PlayMode.cpp
    src/ReferenceFields.cpp
    src/ResourceChoices.cpp
    src/ModelInspection.cpp
    src/ResourceInspection.cpp
    src/TextureInspection.cpp
    src/ViewportProjection.cpp
    src/ResourceInstance.cpp
    src/ResourceLabel.cpp
    src/SceneDocument.cpp
    src/SceneEditing.cpp
    src/Snapping.cpp
    src/SettingsCommands.cpp
    src/SettingsEditing.cpp
    src/NewScene.cpp
    src/ProjectSetup.cpp
    src/ProjectFiles.cpp
    src/ProjectList.cpp
    src/ProjectManager.cpp
    src/SceneCommands.cpp
    src/EditHistory.cpp
    src/LogBuffer.cpp
    src/CompilerDiagnostics.cpp
    src/ScriptToolchain.cpp
    src/ScriptBuild.cpp
    src/ModuleCopies.cpp
    src/render/EditorShaders.cpp
    src/render/BackgroundPass.cpp
    src/render/GridPass.cpp
    src/render/OutlinePass.cpp
)
add_library(yqlora::editor ALIAS yqlora_editor)

target_include_directories(yqlora_editor PUBLIC include PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(yqlora_editor
    PUBLIC yqlora::internal
    PRIVATE nlohmann_json::nlohmann_json
)
yqlora_set_warnings(yqlora_editor)

# Интерфейс редактора на ImGui: панели, диалоги, гизмо, тема. Только вызывает
# логику выше. Программа с main лежит в apps/editor.
add_library(yqlora_editor_ui STATIC
    ui/src/EditorApp.cpp
    ui/src/core/Actions.cpp
    ui/src/core/DefaultLayout.cpp
    ui/src/core/EditorPanel.cpp
    ui/src/core/EditorActions.cpp
    ui/src/core/EditorHost.cpp
    ui/src/dialogs/AddNodeDialog.cpp
    ui/src/dialogs/UnsavedChangesDialog.cpp
    ui/src/gizmos/AxisGizmo.cpp
    ui/src/gizmos/CameraGizmo.cpp
    ui/src/gizmos/GizmoDrawing.cpp
    ui/src/gizmos/GizmoLayer.cpp
    ui/src/gizmos/LightGizmo.cpp
    ui/src/gizmos/GridGizmo.cpp
    ui/src/gizmos/ManipulatorGizmo.cpp
    ui/src/gizmos/SelectionGizmo.cpp
    ui/src/inspectors/MaterialInspector.cpp
    ui/src/inspectors/ModelInspector.cpp
    ui/src/inspectors/ResourceKindInspector.cpp
    ui/src/inspectors/ShaderInspector.cpp
    ui/src/inspectors/TextureInspector.cpp
    ui/src/menu/MainMenu.cpp
    ui/src/panels/FileSystemPanel.cpp
    ui/src/panels/GamePanel.cpp
    ui/src/panels/InspectorPanel.cpp
    ui/src/panels/LogPanel.cpp
    ui/src/panels/ProjectManagerPanel.cpp
    ui/src/panels/ProjectSettingsPanel.cpp
    ui/src/panels/ResourceInspector.cpp
    ui/src/panels/SceneTreePanel.cpp
    ui/src/panels/ViewportPanel.cpp
    ui/src/style/NodeIcons.cpp
    ui/src/style/ResourceIcons.cpp
    ui/src/style/Theme.cpp
    ui/src/widgets/Checkbox.cpp
    ui/src/widgets/ChoiceFields.cpp
    ui/src/widgets/InputControlField.cpp
    ui/src/widgets/LayerFields.cpp
    ui/src/widgets/NameDialog.cpp
    ui/src/widgets/NumberFields.cpp
    ui/src/widgets/PingFlash.cpp
    ui/src/widgets/PlayFrame.cpp
    ui/src/widgets/PropertyContext.cpp
    ui/src/widgets/PropertyEditor.cpp
    ui/src/widgets/PropertyTable.cpp
    ui/src/widgets/ReferenceButton.cpp
    ui/src/widgets/ReleaseClick.cpp
    ui/src/widgets/ReferenceField.cpp
    ui/src/widgets/RenderView.cpp
    ui/src/widgets/ResourceField.cpp
    ui/src/widgets/DragDrop.cpp
    ui/src/widgets/TitleBarButton.cpp
)
add_library(yqlora::editor_ui ALIAS yqlora_editor_ui)

target_include_directories(yqlora_editor_ui PUBLIC ui/include)
target_link_libraries(yqlora_editor_ui
    PUBLIC yqlora::editor
    PRIVATE nlohmann_json::nlohmann_json
)
yqlora_set_warnings(yqlora_editor_ui)
