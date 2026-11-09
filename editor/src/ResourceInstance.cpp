#include "yqlora/editor/ResourceInstance.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/resource/ResourceLoader.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <map>
#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

// Узел без имени в файле.
constexpr const char* kUnnamedNode = "Node3D";
constexpr const char* kUnnamedMesh = "Mesh";

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

std::string meshName(const ResourceDatabase& resources, ResourceId mesh)
{
    const std::string_view name = subresourceName(resources.key(mesh));

    return name.empty() ? kUnnamedMesh : std::string(name);
}

void addModelNode(
    Node3D& parent,
    const ModelData& model,
    std::size_t index,
    const std::map<std::string, ResourceId>& subresources,
    ResourceCache& cache,
    std::size_t depth
)
{
    // Испорченный файл мог бы зациклить дерево на себя.
    if (index >= model.nodes.size() || depth > model.nodes.size()) {
        return;
    }
    const ModelNode& source = model.nodes[index];
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
        addModelNode(added, model, child, subresources, cache, depth + 1);
    }
}

} // namespace

std::unique_ptr<Node> instantiateResource(ResourceId id, ResourceCache& cache, const ResourceDatabase& resources)
{
    if (const ModelData* model = cache.find<ModelData>(id)) {
        auto root = std::make_unique<Node3D>();
        const auto path = resources.path(id);
        root->setName(path ? toUtf8(path->stem()) : kUnnamedNode);
        const auto& subresources = resources.subresources(id);
        for (const std::size_t index : model->roots) {
            addModelNode(*root, *model, index, subresources, cache, 0);
        }

        return root;
    }
    if (cache.find<MeshData>(id)) {
        const ResourceId file = resources.fileOf(id);
        // Встроенный меш не лежит в файле модели: он без материалов.
        const ModelData* model = file != 0 && file != id ? cache.find<ModelData>(file) : nullptr;
        auto node = std::make_unique<Node3D>();
        node->setName(meshName(resources, id));
        addMeshRenderer(*node, id, model, resources.subresources(file), cache);

        return node;
    }

    return nullptr;
}

} // namespace yqlora::editor
