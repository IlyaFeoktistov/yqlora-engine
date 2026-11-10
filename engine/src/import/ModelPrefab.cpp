#include "yqlora/import/ModelPrefab.hpp"

#include "yqlora/core/JsonId.hpp"
#include "yqlora/core/StableHash.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/resource/ResourceLoader.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/ObjectId.hpp"
#include "yqlora/scene/SceneBranches.hpp"
#include "yqlora/scene/SceneFileKeys.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace yqlora {

namespace {

using Json = nlohmann::json;

// Узел без имени в файле.
constexpr const char* kUnnamedNode = "Node3D";
constexpr const char* kUnnamedMesh = "Mesh";
// Ключ корня модели: узла файла с таким ключом нет (у них вид "node:").
constexpr const char* kRootKey = "root";
constexpr std::string_view kNodeKind = "node";
using scene_keys::kChildren;
using scene_keys::kComponents;
using scene_keys::kId;
using scene_keys::kType;

// Слоты меша mesh модели model: подмеш получает вложенный материал, которым
// он покрашен в файле. Без материала или без его ID в .meta (проект только для
// чтения) слот пуст; пустые слоты в конце не пишутся.
std::vector<ResourceRef> modelSlots(
    ResourceId mesh,
    const ModelData& model,
    const std::map<std::string, ResourceId>& subresources,
    ResourceCache& cache
)
{
    std::vector<ResourceRef> slots;
    const MeshData* data = cache.find<MeshData>(mesh);
    if (!data) {
        return slots;
    }
    for (const SubMesh& submesh : data->submeshes) {
        ResourceRef material;
        const auto index = submesh.sourceMaterial;
        if (index && *index >= 0 && static_cast<std::size_t>(*index) < model.materials.size()) {
            if (const auto found = subresources.find(model.materials[*index]); found != subresources.end()) {
                material.id = found->second;
            }
        }
        slots.push_back(material);
    }
    while (!slots.empty() && slots.back().id == 0) {
        slots.pop_back();
    }

    return slots;
}

// Узел с мешем mesh; model - модель, в которой меш лежит (nullptr - не из модели).
void addMeshRenderer(
    Node3D& node,
    ResourceId mesh,
    const ModelData* model,
    const std::map<std::string, ResourceId>& subresources,
    ResourceCache& cache
)
{
    MeshRenderer& renderer = node.addComponent<MeshRenderer>();
    renderer.setMesh(ResourceRef{mesh});
    if (model) {
        renderer.setMaterials(modelSlots(mesh, *model, subresources, cache));
    }
}

// Строит дерево модели и запоминает ключи узлов в порядке обхода (узел, затем
// дети по порядку) - в том же порядке их пишет ветка.
struct ModelTreeBuilder {
    const ModelData& model;
    const std::map<std::string, ResourceId>& subresources;
    ResourceCache& cache;
    SubresourceKeys keys;
    std::vector<std::string> order;

    void addNode(Node3D& parent, std::size_t index, std::size_t depth)
    {
        // Испорченный файл мог бы зациклить дерево на себя.
        if (index >= model.nodes.size() || depth > model.nodes.size()) {
            return;
        }
        const ModelNode& source = model.nodes[index];
        order.push_back(keys.next(kNodeKind, source.name, index));
        auto node = std::make_unique<Node3D>();
        node->setName(source.name.empty() ? kUnnamedNode : source.name);
        node->transform().setPosition(source.position);
        node->transform().setRotation(source.rotation);
        node->transform().setScale(source.scale);
        if (source.mesh && *source.mesh < model.meshes.size()) {
            if (const auto mesh = subresources.find(model.meshes[*source.mesh]); mesh != subresources.end()) {
                addMeshRenderer(*node, mesh->second, &model, subresources, cache);
            }
        }
        Node3D& added = *node;
        parent.addChild(std::move(node));
        for (const std::size_t child : source.children) {
            addNode(added, child, depth + 1);
        }
    }
};

// Выводит ID узлов ветки из ключей (в порядке обхода), ID компонентов - из ID
// узла и типа: у узла модели компонентов разного типа не больше одного.
void assignStableIds(Json& node, const std::vector<std::string>& keys, std::size_t& next)
{
    const std::uint64_t id = next < keys.size() ? stableHash(keys[next]) : newObjectId();
    ++next;
    node[kId] = idToJson(id != 0 ? id : 1);
    if (const auto components = node.find(kComponents); components != node.end()) {
        for (Json& component : *components) {
            component[kId] = idToJson(deriveId(id, stableHash(component.value(kType, ""))));
        }
    }
    if (const auto children = node.find(kChildren); children != node.end()) {
        for (Json& child : *children) {
            assignStableIds(child, keys, next);
        }
    }
}

Json branchWithIds(const TypeRegistry& registry, const Node& root, const std::vector<std::string>& keys)
{
    Json branch = SceneBranches::write(registry, root);
    std::size_t next = 0;
    assignStableIds(branch, keys, next);

    return branch;
}

} // namespace

std::optional<Json> modelBranch(
    ResourceId resource,
    ResourceCache& cache,
    const ResourceDatabase* resources,
    const TypeRegistry& registry
)
{
    static const std::map<std::string, ResourceId> kNoSubresources;
    if (const ModelData* model = cache.find<ModelData>(resource)) {
        const auto& subresources = resources ? resources->subresources(resource) : kNoSubresources;
        auto root = std::make_unique<Node3D>();
        const auto path = resources ? resources->path(resource) : std::nullopt;
        root->setName(path ? toUtf8(path->stem()) : kUnnamedNode);
        ModelTreeBuilder builder{*model, subresources, cache, {}, {kRootKey}};
        for (const std::size_t index : model->roots) {
            builder.addNode(*root, index, 0);
        }

        return branchWithIds(registry, *root, builder.order);
    }
    if (!cache.find<MeshData>(resource)) {
        return std::nullopt;
    }
    const ResourceId file = resources ? resources->fileOf(resource) : 0;
    // Встроенный меш не лежит в файле модели: он без материалов.
    const ModelData* model = file != 0 && file != resource ? cache.find<ModelData>(file) : nullptr;
    const std::string_view key = resources ? resources->key(resource) : std::string_view();
    const std::string_view name = subresourceName(key);
    auto node = std::make_unique<Node3D>();
    node->setName(name.empty() ? kUnnamedMesh : std::string(name));
    addMeshRenderer(*node, resource, model, resources ? resources->subresources(file) : kNoSubresources, cache);

    return branchWithIds(registry, *node, {key.empty() ? std::string(kUnnamedMesh) : std::string(key)});
}

} // namespace yqlora
