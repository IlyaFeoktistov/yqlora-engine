#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/project/Project.hpp>
#include <yqlora/render/GpuResourceCache.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/resource/ResourceLoader.hpp>
#include <yqlora/resource/ResourceMeta.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace yqlora;
namespace fs = std::filesystem;

namespace {

// Данные не меша: ссылка не того вида ничего не находит.
struct TestText {
    static constexpr std::string_view kResourceKind = "test-text";
    std::string text;
};

MeshData triangle(float height)
{
    MeshData mesh;
    mesh.vertices = {{Vec3(0.0f), Vec3(0.0f, 0.0f, 1.0f)}, {Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)}};
    mesh.vertices.push_back({Vec3(0.0f, height, 0.0f), Vec3(0.0f, 0.0f, 1.0f)});
    mesh.indices = {0, 1, 2};
    finishSingleMaterial(mesh);

    return mesh;
}

// Файл .tmesh: "broken" не читается, иначе меш из одного треугольника,
// высота которого - длина текста файла, умноженная на настройку импорта
// "scale". "parts:Door,Door,,Frame" даёт ещё вложенные меши с такими именами
// высотой 1, 2, 3...
class TestLoader final : public ResourceLoader {
public:
    std::span<const std::string_view> extensions() const override
    {
        return kExtensions;
    }

    std::string_view kind() const override
    {
        return MeshData::kResourceKind;
    }

    LoadResult load(const LoadRequest& request) const override
    {
        std::ifstream stream(request.file);
        const std::string content{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        if (content == "broken") {
            return loadError("broken test mesh");
        }
        const float scale = request.import.value("scale", 1.0f);
        LoadResult result;
        result.resources.push_back(
            makeLoadedResource(std::string(), triangle(static_cast<float>(content.size()) * scale))
        );
        constexpr std::string_view kParts = "parts:";
        if (content.starts_with(kParts)) {
            SubresourceKeys keys;
            std::size_t index = 0;
            std::size_t begin = kParts.size();
            while (begin <= content.size()) {
                const std::size_t end = std::min(content.find(',', begin), content.size());
                const std::string name = content.substr(begin, end - begin);
                result.resources.push_back(makeLoadedResource(
                    keys.next(MeshData::kResourceKind, name, index),
                    triangle(static_cast<float>(index + 1))
                ));
                ++index;
                begin = end + 1;
            }
        }

        return result;
    }

private:
    static constexpr std::array<std::string_view, 1> kExtensions{".tmesh"};
};

void write(const fs::path& file, std::string_view content)
{
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::trunc) << content;
}

// Изменение, которое заметит сравнение времени записи даже на файловой
// системе с грубыми отметками времени.
void rewrite(const fs::path& file, std::string_view content)
{
    const auto before = fs::last_write_time(file);
    write(file, content);
    fs::last_write_time(file, before + std::chrono::seconds(2));
}

struct Fixture {
    Fixture()
    {
        directory = fs::temp_directory_path() / "yqlora_tests" / "resource_cache";
        fs::remove_all(directory);
        fs::create_directories(directory);
        write(directory / "models" / "box.TMESH", "abc");
        write(directory / "notes.unknown", "text");
        REQUIRE(Project::create(directory, "Game", registry));
        project.emplace(*Project::open(directory, registry));
        cache.addLoader(std::make_unique<TestLoader>());
        cache.setMetaPolicy(MetaPolicy::Import);
        cache.setProject(&*project);
        box = project->resources().id("models/box.TMESH");
        REQUIRE(box != 0);
    }

    float height(ResourceId id)
    {
        const MeshData* mesh = cache.find<MeshData>(id);

        return mesh ? mesh->vertices[2].position.y : -1.0f;
    }

    test::EngineRegistry registry;
    fs::path directory;
    std::optional<Project> project;
    ResourceCache cache;
    ResourceId box = 0;
};

} // namespace

TEST_CASE("ResourceCache: built-in meshes are always there and of their own kind only")
{
    ResourceCache cache;
    const auto cube = cache.get<MeshData>(builtin::kCubeMesh);
    REQUIRE(cube);
    CHECK(cube.revision != 0);
    CHECK(cache.status(builtin::kCubeMesh)->state == ResourceState::Ready);
    CHECK(cache.find<TestText>(builtin::kCubeMesh) == nullptr);
    CHECK(cache.find<MeshData>(0) == nullptr);
    CHECK(cache.status(0) == nullptr);

    // Без проекта ресурса проекта нет, и это видно в состоянии.
    CHECK(cache.find<MeshData>(builtin::kFirstProjectId) == nullptr);
    CHECK(cache.status(builtin::kFirstProjectId)->state == ResourceState::Missing);
}

TEST_CASE_FIXTURE(Fixture, "ResourceCache: a file loads on first use by the loader of its extension, any case")
{
    CHECK(cache.status(box) == nullptr);
    const auto first = cache.get<MeshData>(box);
    REQUIRE(first);
    CHECK(first.data->vertices[2].position.y == doctest::Approx(3.0f));
    CHECK(cache.status(box)->state == ResourceState::Ready);
    // Второе обращение файл не читает.
    const auto second = cache.get<MeshData>(box);
    CHECK(second.data == first.data);
    CHECK(second.revision == first.revision);

    const ResourceId notes = project->resources().id("notes.unknown");
    CHECK(cache.find<MeshData>(notes) == nullptr);
    CHECK(cache.status(notes)->state == ResourceState::Error);
}

TEST_CASE_FIXTURE(Fixture, "ResourceCache: a reload replaces the data with a new revision, a broken file is an error")
{
    const std::uint64_t loaded = cache.get<MeshData>(box).revision;
    write(directory / "models" / "box.TMESH", "abcde");
    REQUIRE(cache.reload(box));
    const auto reloaded = cache.get<MeshData>(box);
    CHECK(reloaded.revision > loaded);
    CHECK(height(box) == doctest::Approx(5.0f));

    // Прежние данные не выдаются за текущие.
    rewrite(directory / "models" / "box.TMESH", "broken");
    CHECK(cache.reloadChanged() == 1);
    CHECK(cache.find<MeshData>(box) == nullptr);
    CHECK(cache.status(box)->state == ResourceState::Error);
    CHECK(cache.status(box)->message == "broken test mesh");
    CHECK(cache.status(box)->revision > reloaded.revision);

    // Без изменений ничего не перечитывается.
    CHECK(cache.reloadChanged() == 0);
    rewrite(directory / "models" / "box.TMESH", "ab");
    CHECK(cache.reloadChanged() == 1);
    CHECK(height(box) == doctest::Approx(2.0f));
}

TEST_CASE_FIXTURE(Fixture, "ResourceCache: a deleted file is missing and comes back with the file")
{
    REQUIRE(cache.find<MeshData>(box));
    fs::remove(directory / "models" / "box.TMESH");
    CHECK(cache.reloadChanged() == 1);
    CHECK(cache.find<MeshData>(box) == nullptr);
    CHECK(cache.status(box)->state == ResourceState::Missing);

    write(directory / "models" / "box.TMESH", "abcd");
    CHECK(cache.reloadChanged() == 1);
    CHECK(height(box) == doctest::Approx(4.0f));
}

TEST_CASE_FIXTURE(Fixture, "ResourceCache: a renamed file is found by its new path, ID stays")
{
    REQUIRE(cache.find<MeshData>(box));
    fs::rename(directory / "models" / "box.TMESH", directory / "crate.tmesh");
    project->resources().moved("models/box.TMESH", "crate.tmesh");
    CHECK(cache.reloadChanged() == 0);
    rewrite(directory / "crate.tmesh", "abcdef");
    CHECK(height(box) == doctest::Approx(6.0f));
    CHECK(cache.status(box)->state == ResourceState::Ready);
}

TEST_CASE_FIXTURE(Fixture, "ResourceCache: another project forgets resources of the previous one")
{
    REQUIRE(cache.find<MeshData>(box));
    cache.setProject(nullptr);
    CHECK(cache.status(box) == nullptr);
    CHECK(cache.find<MeshData>(box) == nullptr);
    CHECK(cache.find<MeshData>(builtin::kSphereMesh));

    // Ресурса, которого не было в проекте, кэш не помнит после проверки
    // изменений: его могли добавить, и он ищется заново.
    cache.setProject(&*project);
    const ResourceId unknown = builtin::kFirstProjectId;
    CHECK(cache.find<MeshData>(unknown) == nullptr);
    CHECK(cache.status(unknown)->state == ResourceState::Missing);
    cache.reloadChanged();
    CHECK(cache.status(unknown) == nullptr);
}

namespace {

// Копия в "видеопамяти", которая знает, сколько её копий живо.
struct FakeGpu {
    explicit FakeGpu(int& counter, std::uint64_t made)
        : alive(counter)
        , revision(made)
    {
        ++alive;
    }

    ~FakeGpu()
    {
        --alive;
    }

    FakeGpu(const FakeGpu&) = delete;
    FakeGpu& operator=(const FakeGpu&) = delete;

    int& alive;
    std::uint64_t revision;
};

} // namespace

TEST_CASE("GpuResourceCache: a new revision replaces the copy, an unused copy is released")
{
    int alive = 0;
    GpuResourceCache<FakeGpu> gpu;
    const auto get = [&](ResourceId id, std::uint64_t revision) -> FakeGpu& {
        return gpu.get(id, revision, [&] {
            return std::make_unique<FakeGpu>(alive, revision);
        });
    };

    FakeGpu& first = get(1, 10);
    CHECK(&get(1, 10) == &first);
    CHECK(alive == 1);
    CHECK(get(1, 11).revision == 11);
    CHECK(alive == 1);

    get(2, 20);
    for (std::uint64_t frame = 0; frame <= GpuResourceCache<FakeGpu>::kUnusedFramesToKeep; ++frame) {
        get(1, 11);
        gpu.endFrame();
    }
    // Копию 2 давно не рисовали: она ушла, копия 1 осталась.
    CHECK(gpu.size() == 1);
    CHECK(alive == 1);
}

TEST_CASE_FIXTURE(Fixture, "ResourceCache: nested resources get ids from .meta, a removed one is missing")
{
    write(directory / "door.tmesh", "parts:Door,Door,,Frame");
    const ResourceId door = project->resources().add("door.tmesh");
    REQUIRE(cache.find<MeshData>(door));
    const auto& nested = project->resources().subresources(door);
    REQUIRE(nested.size() == 4);
    const ResourceId second = nested.at("mesh:Door#1");
    const ResourceId frame = nested.at("mesh:Frame");
    CHECK(height(second) == doctest::Approx(2.0f));
    CHECK(height(nested.at("mesh:#2")) == doctest::Approx(3.0f));
    CHECK(height(frame) == doctest::Approx(4.0f));

    // Перевыгрузка без второй двери: ID остальных те же, её ID ждёт в .meta.
    rewrite(directory / "door.tmesh", "parts:Door,,Frame");
    CHECK(cache.reloadChanged() == 1);
    CHECK(cache.find<MeshData>(second) == nullptr);
    CHECK(cache.status(second)->state == ResourceState::Missing);
    CHECK(height(frame) == doctest::Approx(3.0f));
    // Безымянный меш ищется по индексу, а индекс сдвинулся: у него новый ключ.
    CHECK(project->resources().subresources(door).size() == 5);

    rewrite(directory / "door.tmesh", "parts:Door,Door,,Frame");
    cache.reloadChanged();
    CHECK(height(second) == doctest::Approx(2.0f));
}

TEST_CASE_FIXTURE(Fixture, "ResourceCache: a read-only cache does not write .meta and skips not imported nested ones")
{
    write(directory / "door.tmesh", "parts:Door,Frame");
    const ResourceId door = project->resources().add("door.tmesh");
    const auto metaText = [&] {
        std::ifstream in(metaPathOf(directory / "door.tmesh"));

        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    const std::string before = metaText();

    ResourceCache game;
    game.addLoader(std::make_unique<TestLoader>());
    game.setProject(&*project);
    REQUIRE(game.find<MeshData>(door));
    CHECK(project->resources().subresources(door).empty());
    CHECK(metaText() == before);

    // Импорт в редакторе даёт ID, после него и игра видит вложенные.
    REQUIRE(cache.find<MeshData>(door));
    const ResourceId frame = project->resources().subresources(door).at("mesh:Frame");
    REQUIRE(game.reload(door));
    CHECK(game.find<MeshData>(frame));
}

TEST_CASE_FIXTURE(Fixture, "ResourceCache: new import settings reach the loader with a new revision")
{
    const auto before = cache.get<MeshData>(box);
    REQUIRE(project->resources().setImportSettings(box, R"({"scale": 2.0})"));
    REQUIRE(cache.reload(box));
    CHECK(cache.status(box)->revision > before.revision);
    CHECK(height(box) == doctest::Approx(6.0f));
}
