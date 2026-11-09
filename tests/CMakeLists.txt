# Модули игры, которые тесты загружают, как редактор загружает модуль проекта.
add_subdirectory(game_module)

add_executable(yqlora_tests
    main.cpp
    test_text_files.cpp
    test_transform.cpp
    test_component.cpp
    test_node.cpp
    test_camera.cpp
    test_lights.cpp
    test_shadows.cpp
    test_scene.cpp
    test_scene_access.cpp
    test_object_ids.cpp
    test_references.cpp
    test_events.cpp
    test_timers.cpp
    test_prefabs.cpp
    test_script.cpp
    test_reflection.cpp
    test_serializer.cpp
    test_materials.cpp
    test_mesh.cpp
    test_input.cpp
    test_editor.cpp
    test_project.cpp
    test_session.cpp
    test_log.cpp
    test_world_transform.cpp
    test_history.cpp
    test_resources.cpp
    test_resource_cache.cpp
    test_gltf.cpp
    test_texture.cpp
    test_resource_editor.cpp
    test_reparent.cpp
    test_snapping.cpp
    test_methods.cpp
    test_runtime_types.cpp
    test_project_files.cpp
    test_project_settings.cpp
    test_property_values.cpp
    test_lifecycle.cpp
    test_localization.cpp
    test_settings.cpp
    test_component_rules.cpp
    test_node_tree.cpp
    test_file_format.cpp
    test_play_document.cpp
    test_project_manager.cpp
    test_frame_clock.cpp
    test_game_module.cpp
    test_missing_types.cpp
    test_log_buffer.cpp
    test_process.cpp
    test_compiler_diagnostics.cpp
    test_script_build.cpp
    test_rhi.cpp
    test_shaders.cpp
    test_shader_resources.cpp
    test_render_graph.cpp
    test_opengl.cpp
    RecordingDevice.cpp
)
target_link_libraries(yqlora_tests PRIVATE yqlora::internal yqlora::editor doctest::doctest stb_headers)
target_compile_definitions(yqlora_tests PRIVATE
    YQLORA_LOCALES_DIR="${PROJECT_SOURCE_DIR}/assets/locales"
    YQLORA_SCRIPT_TEMPLATE_DIR="${PROJECT_SOURCE_DIR}/assets/templates/cpp-script"
    YQLORA_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets"
    YQLORA_CMAKE_COMMAND="${CMAKE_COMMAND}"
    YQLORA_SDK_DIR="$<TARGET_FILE_DIR:yqlora>/sdk"
    YQLORA_TEST_GAME_MODULE="$<TARGET_FILE:yqlora_test_game>"
    YQLORA_THROWING_GAME_MODULE="$<TARGET_FILE:yqlora_throwing_game>"
)
# Тесты загружают его сами, во время работы, поэтому только зависимость сборки.
# SDK: сборки модулей игры в тестах идут против него.
add_dependencies(yqlora_tests yqlora_test_game yqlora_throwing_game yqlora_sdk)
# Примеры тоже проходят через загрузку модуля, если их собирают.
if(TARGET yqlora_example_scripts)
    target_compile_definitions(yqlora_tests PRIVATE
        YQLORA_EXAMPLE_SCRIPTS_MODULE="$<TARGET_FILE:yqlora_example_scripts>"
    )
    add_dependencies(yqlora_tests yqlora_example_scripts)
endif()
yqlora_set_warnings(yqlora_tests)
# Перевод шейдеров в GLSL 330 проверяется без контекста OpenGL. Бэкенд
# спрятан внутри библиотеки движка, поэтому тесты собирают переводчик сами.
if(YQLORA_BACKEND_OPENGL)
    target_sources(yqlora_tests PRIVATE ${PROJECT_SOURCE_DIR}/backends/opengl/src/GlslTranslator.cpp)
    target_include_directories(yqlora_tests PRIVATE ${PROJECT_SOURCE_DIR}/backends/opengl/internal)
    target_link_libraries(yqlora_tests PRIVATE spirv-cross-glsl)
    target_compile_definitions(yqlora_tests PRIVATE YQLORA_HAS_OPENGL=1)
endif()

add_test(NAME yqlora_tests COMMAND yqlora_tests)

# Проект со скриптами из шаблона собирается против SDK так же, как в
# редакторе. Только Ninja: им собирает и редактор.
if(CMAKE_GENERATOR STREQUAL "Ninja")
    add_test(NAME yqlora_game_project
        COMMAND ${CMAKE_COMMAND}
            -DTEMPLATE_DIR=${PROJECT_SOURCE_DIR}/assets/templates/cpp-script
            -DBUILD_DIR=${CMAKE_BINARY_DIR}/tests/game_project
            -DSDK_DIR=$<TARGET_FILE_DIR:yqlora>/sdk
            -DGENERATOR=${CMAKE_GENERATOR}
            -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DLINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/sdk/BuildTemplate.cmake
    )
endif()
