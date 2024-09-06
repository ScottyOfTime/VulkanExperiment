#include "stb_image.h"
#include <iostream>
#include "vk_loader.h"

#include "vk_engine.h"
#include "vk_types.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, const char* filename) {
	ENGINE_MESSAGE_ARGS("Loading glTF", filename);

	tinygltf::TinyGLTF gltfContext;
	tinygltf::Model gltfModel;

	std::string err;
	std::string warn;

	bool res = gltfContext.LoadASCIIFromFile(&gltfModel, &err, &warn, filename);
	if (!warn.empty()) {
		ENGINE_WARNING("glTF loader warning");
	}

	if (!err.empty()) {
		ENGINE_ERROR("glTF loeader error");
	}

	if (!res) {
		ENGINE_ERROR("Failed to load glTF");
	} else {
		ENGINE_MESSAGE_ARGS("Loaded glTF", filename);
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;

	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for (tinygltf::Mesh& mesh : gltfModel.meshes) {
		MeshAsset newmesh;

		newmesh.name = mesh.name;

		// clear the mesh arrays each mesh, we ont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives) {
			GeoSurface newSurface;
			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)gltfModel.accessors[p.indices].count;

			size_t initial_vtx = vertices.size();

			// load indexes
			{
				tinygltf::Accessor indexAccessor = gltfModel.accessors[p.indices];
				indices.reserve(indices.size() + indexAccessor.count);
				tinygltf::BufferView indexBufferView = gltfModel.bufferViews.at(indexAccessor.bufferView);

				for (size_t i = 0; i < indexAccessor.count; i++) {
					indices.push_back(gltfModel.buffers[indexBufferView.buffer].data.at(i + indexBufferView.byteOffset));
				}
			}
			
			// load vertex positions
			{
				tinygltf::Accessor posAccessor = gltfModel.accessors[p.attributes.at("POSITION")];
				vertices.reserve(vertices.size() + posAccessor.count);
				tinygltf::BufferView& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];

				for (size_t i = 0; i < posAccessor.count; i++) {
					glm::vec3 v = (glm::vec3)gltfModel.buffers[posBufferView.buffer].data[i + posBufferView.byteOffset];
					Vertex newvtx;
					// memcpy(&newvtx.position, v, sizeof(newvtx.position));
					newvtx.position = v;
					newvtx.normal = { 1, 0, 0 };
					newvtx.color = glm::vec4 { 1.f };
					newvtx.uv_x = 0;
					newvtx.uv_y = 0;
					vertices[initial_vtx + i] = newvtx;
				}
			}

			for (auto& attrib : p.attributes) {
				tinygltf::Accessor accessor = gltfModel.accessors[attrib.second];
				tinygltf::BufferView bufferView = gltfModel.bufferViews[accessor.bufferView];
				int byteStride = accessor.ByteStride(gltfModel.bufferViews[accessor.bufferView]);

				if (attrib.first.compare("NORMAL") == 0) {
					for (size_t i = 0; i < accessor.count; i++) {
						glm::vec3 v = (glm::vec3)gltfModel.buffers[bufferView.buffer].data.at(i + bufferView.byteOffset);
						vertices[initial_vtx + i].normal = v;
					}
				} else if (attrib.first.compare("TEXCOORD_0") == 0) {
					for (size_t i = 0; i < accessor.count; i++) {
						glm::vec2 v = (glm::vec2)gltfModel.buffers[bufferView.buffer].data.at(i + bufferView.byteOffset);
						vertices[initial_vtx + i].uv_x = v.x;
						vertices[initial_vtx + i].uv_y = v.y;
					}
				} else if (attrib.first.compare("COLOR_0") == 0) {
					for (size_t i = 0; i < accessor.count; i++) {
						glm::vec4 v = (glm::vec4)gltfModel.buffers[bufferView.buffer].data.at(i + bufferView.byteOffset);
					}
				}
			}
		}

		constexpr bool OverrideColors = true;
		if (OverrideColors) {
			for (Vertex& vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.f);
			}
		}
		newmesh.meshBuffers = engine->upload_mesh(indices, vertices);

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
	}

	return meshes;
}
