# Отрисовка ImGui через OpenGL. Отдельной целью, чтобы наши строгие
# предупреждения не применялись к чужому коду.
add_library(imgui_opengl3 STATIC ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_link_libraries(imgui_opengl3 PUBLIC imgui_headers)
target_compile_definitions(imgui_opengl3 PRIVATE "IMGUI_API=${YQLORA_DLL_EXPORT}")

# Шейдеры лежат отдельными файлами и вшиваются в библиотеку при сборке.
set(YQLORA_GL_SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/grid.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline_mask.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline_mask.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/outline.frag
)
set(YQLORA_GL_SHADERS_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/GeneratedShaders.hpp)
# Список через "|": точку с запятой в команде разрезала бы оболочка.
string(REPLACE ";" "|" YQLORA_GL_SHADERS_ARG "${YQLORA_GL_SHADERS}")
add_custom_command(
    OUTPUT ${YQLORA_GL_SHADERS_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${YQLORA_GL_SHADERS_HEADER}
        -DSHADERS=${YQLORA_GL_SHADERS_ARG}
        -P ${PROJECT_SOURCE_DIR}/cmake/EmbedShaders.cmake
    DEPENDS ${YQLORA_GL_SHADERS} ${PROJECT_SOURCE_DIR}/cmake/EmbedShaders.cmake
    COMMENT "Embedding OpenGL shaders"
    VERBATIM
)

add_library(yqlora_opengl STATIC
    ${YQLORA_GL_SHADERS_HEADER}
    src/OpenGLRenderer.cpp
    src/GlBuffer.cpp
    src/GlCommandList.cpp
    src/GlDevice.cpp
    src/GlShader.cpp
    src/GlRenderTarget.cpp
    src/GlUiRenderer.cpp
    src/GridPass.cpp
    src/GlTexture.cpp
    src/MeshPass.cpp
    src/OutlinePass.cpp
)
add_library(yqlora::opengl ALIAS yqlora_opengl)

# Собирается внутрь библиотеки движка. Заголовок бэкенда виден только фабрике рендереров.
target_include_directories(yqlora_opengl PUBLIC internal PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(yqlora_opengl
    PUBLIC yqlora_engine_code
    PRIVATE glad SDL3::SDL3-static imgui_opengl3
)
yqlora_set_warnings(yqlora_opengl)
