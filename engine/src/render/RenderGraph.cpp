#include "yqlora/render/RenderGraph.hpp"

#include <algorithm>
#include <queue>
#include <stdexcept>

namespace yqlora {

namespace {

bool sameKind(const rhi::TextureDesc& a, const rhi::TextureDesc& b)
{
    return a.width == b.width && a.height == b.height && a.format == b.format && a.usage == b.usage
           && a.mipmaps == b.mipmaps;
}

} // namespace

rhi::Texture& TransientTextures::acquire(const rhi::TextureDesc& desc)
{
    for (Entry& entry : m_entries) {
        if (!entry.busy && sameKind(entry.texture->desc(), desc)) {
            entry.busy = true;
            entry.lastUsedFrame = m_frame;

            return *entry.texture;
        }
    }
    m_entries.push_back({m_device.createTexture(desc), m_frame, true});

    return *m_entries.back().texture;
}

void TransientTextures::endFrame()
{
    ++m_frame;
    std::erase_if(m_entries, [&](const Entry& entry) {
        return m_frame - entry.lastUsedFrame > kUnusedFramesToKeep;
    });
    for (Entry& entry : m_entries) {
        entry.busy = false;
    }
}

RenderGraph::TextureId RenderGraph::import(rhi::Texture& texture, std::optional<rhi::ResourceState> finalState)
{
    m_textures.push_back({&texture, texture.desc(), true, finalState});

    return static_cast<TextureId>(m_textures.size() - 1);
}

RenderGraph::TextureId RenderGraph::create(const rhi::TextureDesc& desc)
{
    m_textures.push_back({nullptr, desc, false, std::nullopt});

    return static_cast<TextureId>(m_textures.size() - 1);
}

const rhi::TextureDesc& RenderGraph::desc(TextureId id) const
{
    return m_textures.at(id).desc;
}

void RenderGraph::addPass(Pass pass)
{
    m_passes.push_back(std::move(pass));
}

rhi::Texture& RenderGraph::texture(TextureId id) const
{
    rhi::Texture* texture = m_textures.at(id).texture;
    if (!texture) {
        throw std::logic_error("RenderGraph: a transient texture exists only while the graph runs");
    }

    return *texture;
}

std::vector<std::size_t> RenderGraph::order() const
{
    const std::size_t count = m_passes.size();
    std::vector<std::vector<std::size_t>> writers(m_textures.size());
    for (std::size_t i = 0; i < count; ++i) {
        writers.at(m_passes[i].color.texture).push_back(i);
        if (m_passes[i].depth) {
            writers.at(m_passes[i].depth->texture).push_back(i);
        }
    }

    std::vector<std::vector<std::size_t>> next(count);
    std::vector<std::size_t> waiting(count, 0);
    const auto edge = [&](std::size_t from, std::size_t to) {
        next[from].push_back(to);
        ++waiting[to];
    };
    // Пишущие в одну текстуру рисуют друг поверх друга в порядке объявления.
    for (const std::vector<std::size_t>& list : writers) {
        for (std::size_t i = 1; i < list.size(); ++i) {
            edge(list[i - 1], list[i]);
        }
    }
    // Читающий - после всех, кто пишет.
    for (std::size_t i = 0; i < count; ++i) {
        for (const TextureId read : m_passes[i].reads) {
            const std::vector<std::size_t>& list = writers.at(read);
            if (list.empty() && !m_textures[read].imported) {
                throw std::logic_error(
                    "RenderGraph: pass '" + m_passes[i].name + "' reads a texture that no pass writes"
                );
            }
            for (const std::size_t writer : list) {
                if (writer == i) {
                    throw std::logic_error(
                        "RenderGraph: pass '" + m_passes[i].name + "' reads a texture it draws into"
                    );
                }
                edge(writer, i);
            }
        }
    }

    // Без зависимостей - в порядке добавления.
    std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<>> ready;
    for (std::size_t i = 0; i < count; ++i) {
        if (waiting[i] == 0) {
            ready.push(i);
        }
    }
    std::vector<std::size_t> result;
    result.reserve(count);
    while (!ready.empty()) {
        const std::size_t pass = ready.top();
        ready.pop();
        result.push_back(pass);
        for (const std::size_t after : next[pass]) {
            if (--waiting[after] == 0) {
                ready.push(after);
            }
        }
    }
    if (result.size() != count) {
        std::string names;
        for (std::size_t i = 0; i < count; ++i) {
            if (waiting[i] != 0) {
                names += (names.empty() ? "'" : ", '") + m_passes[i].name + "'";
            }
        }
        throw std::logic_error("RenderGraph: passes depend on each other in a cycle: " + names);
    }

    return result;
}

void RenderGraph::execute(rhi::CommandList& commands)
{
    const std::vector<std::size_t> passes = order();
    const auto use = [&](TextureId id, rhi::ResourceState state) -> rhi::Texture& {
        Texture& entry = m_textures.at(id);
        if (!entry.texture) {
            entry.texture = &m_transients.acquire(entry.desc);
        }
        commands.transition(*entry.texture, state);

        return *entry.texture;
    };

    for (const std::size_t index : passes) {
        const Pass& pass = m_passes[index];
        for (const TextureId read : pass.reads) {
            use(read, rhi::ResourceState::ShaderRead);
        }
        rhi::Texture& color = use(pass.color.texture, rhi::ResourceState::ColorTarget);
        rhi::Texture* depth = pass.depth ? &use(pass.depth->texture, rhi::ResourceState::DepthTarget) : nullptr;

        commands.beginRenderPass({
            .color = &color,
            .depth = depth,
            .colorLoad = pass.color.load,
            .depthLoad = pass.depth ? pass.depth->load : rhi::LoadOp::Load,
            .clearColor = pass.clearColor,
        });
        if (pass.record) {
            pass.record(commands, *this);
        }
        commands.endRenderPass();
    }

    for (const Texture& entry : m_textures) {
        if (entry.imported && entry.finalState) {
            commands.transition(*entry.texture, *entry.finalState);
        }
    }
}

} // namespace yqlora
