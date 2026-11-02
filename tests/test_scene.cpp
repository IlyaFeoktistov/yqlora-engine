#include <doctest/doctest.h>

#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/scene/Scene.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

using namespace yqlora;

TEST_CASE("Scene: new scene is empty")
{
    Scene scene{EngineServices::none()};
    CHECK(scene.empty());
    CHECK(scene.root() == nullptr);

    // Пустую сцену можно обновлять и рисовать.
    scene.advance(0.016f);
    const FrameData frame = extractFrame(scene, 1.0f);
    CHECK(frame.views.empty());
    CHECK(frame.drawItems.empty());
}

TEST_CASE("Scene: root can be of any node type")
{
    Scene scene{EngineServices::none()};
    auto& root = scene.createRoot<Node3D>("World");
    CHECK_FALSE(scene.empty());
    CHECK(scene.root() == &root);
    CHECK(root.parent() == nullptr);
}

TEST_CASE("Scene: takeRoot leaves the scene empty")
{
    Scene scene{EngineServices::none()};
    auto& root = scene.createRoot<Node>();

    auto owned = scene.takeRoot();
    CHECK(owned.get() == &root);
    CHECK(scene.empty());
}

namespace {

// Записывает, когда узлы и компоненты входят в сцену и выходят из неё.
std::vector<std::string> g_events;

struct Tracked final : Node3D {
    YQLORA_TYPE(Tracked, Node3D)

    using Node3D::Node3D;

    void onEnterScene() override
    {
        g_events.push_back("enter " + name());
    }

    void onExitScene() override
    {
        g_events.push_back("exit " + name());
    }
};

struct Sensor final : Component {
    YQLORA_TYPE(Sensor, Component)

    void onEnterScene() override
    {
        g_events.push_back("sensor enter");
    }

    void onExitScene() override
    {
        g_events.push_back("sensor exit");
    }
};

} // namespace

TEST_CASE("Scene: nodes know their scene, enter parent first and exit children first")
{
    g_events.clear();
    auto root = std::make_unique<Tracked>("Root");
    root->addChild<Tracked>("Child");
    CHECK(root->scene() == nullptr);

    Scene scene{EngineServices::none()};
    Node& rootRef = *root;
    scene.setRoot(std::move(root));
    CHECK(rootRef.scene() == &scene);
    CHECK(rootRef.children()[0]->scene() == &scene);
    CHECK(g_events == std::vector<std::string>{"enter Root", "enter Child"});

    g_events.clear();
    scene.setRoot(nullptr);
    CHECK(g_events == std::vector<std::string>{"exit Child", "exit Root"});
}

TEST_CASE("Scene: components are found by type while they are in the scene")
{
    g_events.clear();
    Scene scene{EngineServices::none()};
    auto& root = scene.createRoot<Node3D>("Root");
    auto& sensor = root.addComponent<Sensor>();
    CHECK(g_events == std::vector<std::string>{"sensor enter"});

    // Ветка вне сцены в индекс не попадает, а при добавлении в дерево - попадает.
    auto branch = std::make_unique<Node3D>("Branch");
    auto& second = branch->addComponent<Sensor>();
    CHECK(std::ranges::distance(scene.components<Sensor>()) == 1);
    root.addChild(std::move(branch));
    const auto sensors = scene.components<Sensor>();
    CHECK(std::ranges::distance(sensors) == 2);
    CHECK(*sensors.begin() == &sensor);

    root.removeComponent(sensor);
    CHECK(std::ranges::distance(scene.components<Sensor>()) == 1);
    CHECK(*scene.components<Sensor>().begin() == &second);

    auto removed = root.removeChild(*root.children()[0]);
    CHECK(std::ranges::empty(scene.components<Sensor>()));
    CHECK(removed->scene() == nullptr);
    CHECK(g_events.back() == "sensor exit");
}
