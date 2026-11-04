#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/Device.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace yqlora {

// Временные текстуры графа (маска контура и подобные) между кадрами: граф
// берёт текстуру нужного вида на кадр и возвращает её; та, что не
// понадобилась kUnusedFramesToKeep кадров, освобождается.
class YQLORA_API TransientTextures {
public:
    static constexpr std::uint64_t kUnusedFramesToKeep = 60;

    explicit TransientTextures(rhi::Device& device)
        : m_device(device)
    {
    }

    // Свободная текстура такого вида, иначе новая. Занята до endFrame.
    rhi::Texture& acquire(const rhi::TextureDesc& desc);

    // Кадр показан: все текстуры снова свободны, давно не нужные удаляются.
    void endFrame();

    std::size_t size() const
    {
        return m_entries.size();
    }

private:
    struct Entry {
        std::unique_ptr<rhi::Texture> texture;
        std::uint64_t lastUsedFrame = 0;
        bool busy = false;
    };

    rhi::Device& m_device;
    std::vector<Entry> m_entries;
    std::uint64_t m_frame = 0;
};

// Граф проходов одного кадра в объёме MVP. Проход объявляет, в какие
// текстуры рисует и какие читает; граф упорядочивает проходы (читающий -
// после всех, кто пишет; пишущие в одну текстуру - в порядке объявления),
// создаёт временные текстуры, переводит текстуры в нужные состояния и
// открывает и закрывает проходы. Цикл и чтение временной текстуры, в
// которую никто не пишет, - std::logic_error в execute.
class YQLORA_API RenderGraph {
public:
    using TextureId = std::uint32_t;

    struct Target {
        TextureId texture = 0;
        rhi::LoadOp load = rhi::LoadOp::Load;
        // Грань кубической текстуры и мип, куда рисует проход (только цвет).
        int face = 0;
        int mip = 0;
    };

    struct Pass {
        std::string name{};
        Target color{};
        std::optional<Target> depth{};
        Vec4 clearColor{0.0f, 0.0f, 0.0f, 1.0f};
        // Читаются шейдерами прохода (состояние ShaderRead).
        std::vector<TextureId> reads{};
        // Пишет команды внутри открытого прохода.
        std::function<void(rhi::CommandList& commands, const RenderGraph& graph)> record{};
        // Вместо прохода рисования: мипы color.texture из её мипа 0. Для
        // порядка это запись в текстуру, как у прохода, который в неё рисует.
        bool generateMipmaps = false;
    };

    explicit RenderGraph(TransientTextures& transients)
        : m_transients(transients)
    {
    }

    // Текстура извне графа (задний буфер, цель рендера). Её содержимое
    // считается записанным. finalState - в каком состоянии оставить её после
    // кадра; nullopt - в том, в каком её оставил последний проход.
    TextureId import(rhi::Texture& texture, std::optional<rhi::ResourceState> finalState = std::nullopt);

    // Временная текстура на кадр; появляется у первого прохода, который в неё пишет.
    TextureId create(const rhi::TextureDesc& desc);

    const rhi::TextureDesc& desc(TextureId id) const;

    void addPass(Pass pass);

    // Текстура на время record; у временной - только в execute.
    rhi::Texture& texture(TextureId id) const;

    // Порядок проходов по зависимостям (индексы в порядке добавления).
    std::vector<std::size_t> order() const;

    // Исполняет проходы в порядке order() в commands.
    void execute(rhi::CommandList& commands);

private:
    struct Texture {
        rhi::Texture* texture = nullptr;
        rhi::TextureDesc desc{};
        bool imported = false;
        std::optional<rhi::ResourceState> finalState{};
    };

    TransientTextures& m_transients;
    std::vector<Texture> m_textures;
    std::vector<Pass> m_passes;
};

} // namespace yqlora
