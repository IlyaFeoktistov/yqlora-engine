# Библиотека движка yqlora (.so или .dll): ядро, Application, рендереры,
# ImGui. Её подключают редактор, запуск игры и dll со скриптами, так что
# состояние движка (лог, локализация, контекст ImGui) у всех одно.

# Заголовки и настройки для кода, который собирается внутрь библиотеки:
# сам движок и статические бэкенды.
add_library(yqlora_engine_code INTERFACE)
target_include_directories(yqlora_engine_code INTERFACE include internal)
target_link_libraries(yqlora_engine_code INTERFACE glm::glm imgui_headers)
target_compile_definitions(yqlora_engine_code INTERFACE
    ${YQLORA_PUBLIC_DEFINITIONS}
    YQLORA_BUILDING_ENGINE
    "IMGUI_API=${YQLORA_DLL_EXPORT}"
)

# Шейдеры движка лежат файлами в shaders/ и вшиваются в библиотеку при сборке.
set(YQLORA_ENGINE_SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/yqlora/uniforms.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include/yqlora/vertex.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline_mask.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline_mask.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline.frag
)
set(YQLORA_ENGINE_SHADERS_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedShaders.hpp)
# Список через "|": точку с запятой в команде разрезала бы оболочка.
string(REPLACE ";" "|" YQLORA_ENGINE_SHADERS_ARG "${YQLORA_ENGINE_SHADERS}")
add_custom_command(
    OUTPUT ${YQLORA_ENGINE_SHADERS_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${YQLORA_ENGINE_SHADERS_HEADER}
        -DROOT=${CMAKE_CURRENT_SOURCE_DIR}/shaders
        -DSHADERS=${YQLORA_ENGINE_SHADERS_ARG}
        -P ${PROJECT_SOURCE_DIR}/cmake/EmbedShaders.cmake
    DEPENDS ${YQLORA_ENGINE_SHADERS} ${PROJECT_SOURCE_DIR}/cmake/EmbedShaders.cmake
    COMMENT "Embedding engine shaders"
    VERBATIM
)

add_library(yqlora SHARED
    ${YQLORA_ENGINE_SHADERS_HEADER}
    src/core/Application.cpp
    src/core/AppPaths.cpp
    src/core/EngineServices.cpp
    src/core/EngineTypes.cpp
    src/core/FileFormat.cpp
    src/core/FrameClock.cpp
    src/core/FrameLoop.cpp
    src/core/Log.cpp
    src/core/Settings.cpp
    src/core/Version.cpp
    src/i18n/Language.cpp
    src/i18n/Localization.cpp
    src/platform/FileDialogs.cpp
    src/platform/Gamepads.cpp
    src/platform/Platform.cpp
    src/platform/Window.cpp
    src/platform/InputFeeder.cpp
    src/platform/MouseWrap.cpp
    src/platform/SharedLibrary.cpp
    src/platform/Process.cpp
    src/import/Cgltf.cpp
    src/import/EngineLoaders.cpp
    src/import/GltfLoader.cpp
    src/import/ImageLoader.cpp
    src/import/StbImage.cpp
    src/import/TextureImport.cpp
    src/input/Input.cpp
    src/input/InputControl.cpp
    src/input/InputSettings.cpp
    src/input/InputTypes.cpp
    src/math/Euler.cpp
    src/scene/ExecutionOrder.cpp
    src/scene/ExecutionOrderSettings.cpp
    src/scene/GameTime.cpp
    src/scene/LayerSettings.cpp
    src/scene/MissingComponent.cpp
    src/scene/Node.cpp
    src/scene/NodeTree.cpp
    src/scene/Component.cpp
    src/scene/ComponentIndex.cpp
    src/scene/ComponentLifecycle.cpp
    src/scene/Transform3D.cpp
    src/scene/Node3D.cpp
    src/scene/PhaseList.cpp
    src/scene/Scene.cpp
    src/scene/SceneSystem.cpp
    src/scene/SceneSystems.cpp
    src/scene/TransformSystem3D.cpp
    src/scene/TreeChangeQueue.cpp
    src/scene/SceneSerializer.cpp
    src/scene/SceneTypes.cpp
    src/scene/TimeSettings.cpp
    src/scene/UnknownProperties.cpp
    src/project/Project.cpp
    src/project/ProjectSettings.cpp
    src/project/ProjectTypes.cpp
    src/project/ProjectUpgrade.cpp
    src/project/SettingsSection.cpp
    src/reflection/ComponentRules.cpp
    src/reflection/Method.cpp
    src/reflection/Property.cpp
    src/reflection/PropertyJson.cpp
    src/reflection/TypeBuilder.cpp
    src/reflection/TypeInfo.cpp
    src/reflection/TypeRegistry.cpp
    src/render/Camera3D.cpp
    src/render/FrameBuilder.cpp
    src/render/GpuMesh.cpp
    src/render/GraphicsBackend.cpp
    src/render/GridPass.cpp
    src/render/MeshPass.cpp
    src/render/OutlinePass.cpp
    src/render/Renderer.cpp
    src/render/RenderTarget.cpp
    src/render/GraphicsSettings.cpp
    src/render/BuiltinMeshes.cpp
    src/render/MeshData.cpp
    src/render/PrimitiveMeshes.cpp
    src/render/RenderSystem.cpp
    src/render/RenderTypes.cpp
    src/render/UniformRing.cpp
    src/rhi/Pipeline.cpp
    src/resource/ResourceCache.cpp
    src/resource/ResourceDatabase.cpp
    src/resource/ResourceLoader.cpp
    src/runtime/GameRuntime.cpp
    src/shader/EngineShaders.cpp
    src/shader/ShaderBuild.cpp
    src/shader/ShaderCompiler.cpp
    src/shader/ShaderReflect.cpp
    src/shader/UniformLayout.cpp
    src/scripting/GameModule.cpp
    src/scripting/ModuleRegistrar.cpp
    src/scripting/Script.cpp
    src/scripting/ScriptingTypes.cpp
    src/ui/UiLayer.cpp
)
add_library(yqlora::yqlora ALIAS yqlora)

# include/ - публичный API для игр: математика, сцена, ввод, лог, tr().
# internal/ - окно, платформа, рендер, Application: см. yqlora::internal ниже.
target_include_directories(yqlora PUBLIC include PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)
# glslang - компилятор шейдеров, SPIRV-Cross (ядро) - их отражение.
target_link_libraries(yqlora
    PUBLIC glm::glm imgui_headers
    PRIVATE
        yqlora_engine_code imgui SDL3::SDL3-static nlohmann_json::nlohmann_json cgltf_headers stb_headers
        glslang glslang-default-resource-limits spirv-cross-core
)
target_compile_definitions(yqlora PRIVATE YQLORA_ENGINE_VERSION="${PROJECT_VERSION}")
target_compile_definitions(yqlora INTERFACE
    ${YQLORA_PUBLIC_DEFINITIONS}
    "IMGUI_API=${YQLORA_DLL_IMPORT}"
)
if(UNIX AND NOT APPLE)
    # SDL помечает свои функции видимыми даже в статической сборке: прячем
    # всё, что пришло из статических архивов, наружу им незачем.
    target_link_options(yqlora PRIVATE "LINKER:--exclude-libs,ALL")
endif()
yqlora_set_warnings(yqlora)

# Доступ к внутренним заголовкам: для программы с редактором и тестов движка.
# Игровой код подключает только yqlora::yqlora.
add_library(yqlora_internal INTERFACE)
add_library(yqlora::internal ALIAS yqlora_internal)
target_include_directories(yqlora_internal INTERFACE internal)
# Внутренние заголовки построены на nlohmann/json (только заголовки).
target_link_libraries(yqlora_internal INTERFACE yqlora::yqlora nlohmann_json::nlohmann_json)

include(${PROJECT_SOURCE_DIR}/cmake/Sdk.cmake)
