#include "yqlora/import/GltfNodes.hpp"

#include <cgltf.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace yqlora {

ModelNode readGltfNode(const cgltf_data& data, std::size_t index, float scale)
{
    const cgltf_node& node = data.nodes[index];
    ModelNode result;
    result.name = node.name ? node.name : "";
    if (node.has_matrix) {
        Vec3 skew;
        Vec4 perspective;
        glm::decompose(glm::make_mat4(node.matrix), result.scale, result.rotation, result.position, skew, perspective);
    } else {
        if (node.has_translation) {
            result.position = glm::make_vec3(node.translation);
        }
        if (node.has_rotation) {
            // В glTF кватернион - x, y, z, w; glm ждёт w первым.
            result.rotation = Quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        }
        if (node.has_scale) {
            result.scale = glm::make_vec3(node.scale);
        }
    }
    result.position *= scale;
    if (node.mesh) {
        result.mesh = cgltf_mesh_index(&data, node.mesh);
    }
    for (cgltf_size i = 0; i < node.children_count; ++i) {
        result.children.push_back(cgltf_node_index(&data, node.children[i]));
    }

    return result;
}

std::vector<std::size_t> gltfRoots(const cgltf_data& data)
{
    std::vector<std::size_t> roots;
    const cgltf_scene* scene = data.scene ? data.scene : (data.scenes_count > 0 ? data.scenes : nullptr);
    if (scene) {
        for (cgltf_size i = 0; i < scene->nodes_count; ++i) {
            roots.push_back(cgltf_node_index(&data, scene->nodes[i]));
        }
    } else {
        for (cgltf_size n = 0; n < data.nodes_count; ++n) {
            if (!data.nodes[n].parent) {
                roots.push_back(n);
            }
        }
    }

    return roots;
}

} // namespace yqlora
