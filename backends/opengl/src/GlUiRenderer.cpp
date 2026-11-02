#include "yqlora/opengl/GlUiRenderer.hpp"

#include "yqlora/opengl/GlDevice.hpp"

#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <string>

namespace yqlora::opengl {

GlUiRenderer::GlUiRenderer(SDL_Window* window, SDL_GLContextState* context)
{
    ImGui_ImplSDL3_InitForOpenGL(window, context);
    // Шейдеры ImGui под ту же версию GLSL, что и контекст.
    const std::string glslVersion = "#version " + std::to_string(GlDevice::kVersionMajor)
                                    + std::to_string(GlDevice::kVersionMinor) + "0 core";
    ImGui_ImplOpenGL3_Init(glslVersion.c_str());
}

GlUiRenderer::~GlUiRenderer()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

void GlUiRenderer::beginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void GlUiRenderer::render()
{
    const ImDrawData* data = ImGui::GetDrawData();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(
        0,
        0,
        static_cast<GLsizei>(data->DisplaySize.x * data->FramebufferScale.x),
        static_cast<GLsizei>(data->DisplaySize.y * data->FramebufferScale.y)
    );
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace yqlora::opengl
