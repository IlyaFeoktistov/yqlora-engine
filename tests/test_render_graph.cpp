#include "RecordingDevice.hpp"

#include <doctest/doctest.h>

#include <yqlora/render/Prepare.hpp>
#include <yqlora/render/RenderGraph.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

using namespace yqlora;
using yqlora::test::CommandKind;
using yqlora::test::RecordingDevice;

namespace {

const rhi::TextureDesc kMaskDesc{
    .width = 8,
    .height = 8,
    .format = rhi::Format::R8,
    .usage = rhi::kColorTarget | rhi::kSampled,
};

// Проход, который только отмечает, что его записали.
RenderGraph::Pass pass(
    std::string name,
    RenderGraph::TextureId color,
    std::vector<RenderGraph::TextureId> reads,
    std::vector<std::string>& log
)
{
    return {
        .name = name,
        .color = {color, rhi::LoadOp::Load},
        .reads = std::move(reads),
        .record = [name, &log](rhi::CommandList&, const RenderGraph&) {
            log.push_back(name);
        },
    };
}

CameraData lookingDownMinusZ()
{
    return {
        .view = glm::lookAt(Vec3(0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f)),
        .projection = math::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f),
        .position = Vec3(0.0f),
    };
}

DrawItem itemAt(const Vec3& position, int layer = 0)
{
    return {.bounds = {position - Vec3(0.5f), position + Vec3(0.5f)}, .layer = layer};
}

} // namespace

TEST_CASE("RenderGraph: a reader goes after the passes that write its texture")
{
    RecordingDevice device;
    TransientTextures transients(device);
    RenderGraph graph(transients);
    const auto color = graph.import(device.backBuffer());
    const auto mask = graph.create(kMaskDesc);

    std::vector<std::string> log;
    // Объявлены не по порядку: контур читает маску, которую рисуют позже.
    graph.addPass(pass("scene", color, {}, log));
    graph.addPass(pass("outline", color, {mask}, log));
    graph.addPass(pass("mask", mask, {}, log));
    graph.execute(device.commands());

    CHECK(log == std::vector<std::string>{"scene", "mask", "outline"});
    CHECK(transients.size() == 1);
}

TEST_CASE("RenderGraph: passes into one texture keep the order they were added in")
{
    RecordingDevice device;
    TransientTextures transients(device);
    RenderGraph graph(transients);
    const auto color = graph.import(device.backBuffer());
    std::vector<std::string> log;
    graph.addPass(pass("first camera", color, {}, log));
    graph.addPass(pass("second camera", color, {}, log));
    graph.addPass(pass("grid", color, {}, log));
    graph.execute(device.commands());

    CHECK(log == std::vector<std::string>{"first camera", "second camera", "grid"});
}

TEST_CASE("RenderGraph: a cycle and a read of an unwritten texture are errors")
{
    RecordingDevice device;
    TransientTextures transients(device);
    std::vector<std::string> log;

    SUBCASE("cycle")
    {
        RenderGraph graph(transients);
        const auto a = graph.create(kMaskDesc);
        const auto b = graph.create(kMaskDesc);
        graph.addPass(pass("a", a, {b}, log));
        graph.addPass(pass("b", b, {a}, log));
        CHECK_THROWS_WITH_AS(graph.execute(device.commands()), doctest::Contains("cycle"), std::logic_error);
    }
    SUBCASE("unwritten")
    {
        RenderGraph graph(transients);
        const auto color = graph.import(device.backBuffer());
        const auto mask = graph.create(kMaskDesc);
        graph.addPass(pass("outline", color, {mask}, log));
        CHECK_THROWS_WITH_AS(graph.order(), doctest::Contains("no pass writes"), std::logic_error);
    }
    SUBCASE("reads its own target")
    {
        RenderGraph graph(transients);
        const auto mask = graph.create(kMaskDesc);
        graph.addPass(pass("feedback", mask, {mask}, log));
        CHECK_THROWS_AS(graph.order(), std::logic_error);
    }
    CHECK(log.empty());
}

TEST_CASE("RenderGraph: states change between passes and imported textures end where asked")
{
    RecordingDevice device;
    TransientTextures transients(device);
    auto target = device.createTexture({.width = 8, .height = 8, .usage = rhi::kColorTarget | rhi::kSampled}, {});
    device.commands().transition(*target, rhi::ResourceState::ShaderRead);

    RenderGraph graph(transients);
    const auto color = graph.import(*target, rhi::ResourceState::ShaderRead);
    const auto mask = graph.create(kMaskDesc);
    std::vector<std::string> log;
    graph.addPass(pass("mask", mask, {}, log));
    graph.addPass(pass("outline", color, {mask}, log));
    CHECK_NOTHROW(graph.execute(device.commands()));
    CHECK(target->state() == rhi::ResourceState::ShaderRead);
}

TEST_CASE("TransientTextures: a texture is reused next frame and freed when not needed")
{
    RecordingDevice device;
    TransientTextures transients(device);
    rhi::Texture& first = transients.acquire(kMaskDesc);
    // В одном кадре - разные текстуры.
    CHECK(&transients.acquire(kMaskDesc) != &first);
    transients.endFrame();
    CHECK(&transients.acquire(kMaskDesc) == &first);
    CHECK(transients.size() == 2);

    for (std::uint64_t i = 0; i <= TransientTextures::kUnusedFramesToKeep + 1; ++i) {
        transients.endFrame();
    }
    CHECK(transients.size() == 0);
}

TEST_CASE("Prepare: a view keeps what is in its frustum and mask, nearest first")
{
    const std::vector<DrawItem> items{
        itemAt({0.0f, 0.0f, -10.0f}),
        itemAt({0.0f, 0.0f, 10.0f}), // за спиной
        itemAt({0.0f, 0.0f, -3.0f}),
        itemAt({100.0f, 0.0f, -10.0f}), // сбоку за краем
        itemAt({0.0f, 0.0f, -200.0f}),  // дальше дальней плоскости
        itemAt({0.0f, 0.0f, -5.0f}, 3), // слой не в маске
    };
    // Все слои, кроме третьего.
    const ViewData view{.camera = lookingDownMinusZ(), .mask = kAllLayers & ~(LayerMask{1} << 3)};

    const std::vector<const DrawItem*> visible = prepareView(items, view);
    REQUIRE(visible.size() == 2);
    CHECK(visible[0] == &items[2]);
    CHECK(visible[1] == &items[0]);
}

TEST_CASE("Prepare: bounds follow the transform")
{
    const Bounds unit{Vec3(-0.5f), Vec3(0.5f)};
    const Mat4 moved = glm::translate(Mat4(1.0f), Vec3(10.0f, 0.0f, 0.0f));
    const Bounds result = transformBounds(unit, glm::scale(moved, Vec3(2.0f)));
    CHECK(math::nearlyEqual(result.min, Vec3(9.0f, -1.0f, -1.0f)));
    CHECK(math::nearlyEqual(result.max, Vec3(11.0f, 1.0f, 1.0f)));

    // Повёрнутая на 45 градусов коробка шире.
    const Bounds turned = transformBounds(unit, glm::rotate(Mat4(1.0f), glm::radians(45.0f), Vec3(0.0f, 1.0f, 0.0f)));
    CHECK(turned.max.x == doctest::Approx(std::sqrt(0.5f)));
}
