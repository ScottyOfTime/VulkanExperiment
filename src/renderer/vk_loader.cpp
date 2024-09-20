#include <iostream>
#include "vk_loader.h"

#include "vk_engine.h"
#include "vk_types.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, const char* filename) {
	ENGINE_MESSAGE_ARGS("Loading glTF %s", filename);

	tinygltf::TinyGLTF gltfContext;
	tinygltf::Model gltfModel;

	std::string err;
	std::string warn;

	bool res = gltfContext.LoadBinaryFromFile(&gltfModel, &err, &warn, filename);
	if (!warn.empty()) {
		ENGINE_WARNING("glTF loader warning");
	}

	if (!err.empty()) {
		ENGINE_ERROR("glTF loeader error");
	}

	if (!res) {
		ENGINE_ERROR("Failed to load glTF");
	} else {
		ENGINE_MESSAGE_ARGS("Loaded glTF %s", filename);
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;

	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	int mesh_count = 0;
	for (tinygltf::Mesh& mesh : gltfModel.meshes) {
		mesh_count++;
		MeshAsset newmesh;

		newmesh.name = mesh.name;

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (size_t i = 0; i < mesh.primitives.size(); i++) {
			const tinygltf::Primitive& p = mesh.primitives[i];
			GeoSurface newSurface;
			newSurface.startIndex = static_cast<uint32_t>(indices.size());
			newSurface.count = static_cast<uint32_t>(gltfModel.accessors[p.indices].count);
			uint32_t vertexStart = static_cast<uint32_t>(vertices.size());
			uint32_t indexCount = 0;

			// vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				size_t vertexCount = 0;

				// get buffer data for vertex positions
				if (p.attributes.find("POSITION") != p.attributes.end()) {
					const tinygltf::Accessor& accessor = gltfModel.accessors[p.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}

				// get buffer data for vertex normals
				if (p.attributes.find("NORMAL") != p.attributes.end()) {
					const tinygltf::Accessor& accessor = gltfModel.accessors[p.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// get buffer data for vertex texture coordinates
				if (p.attributes.find("TEXCOORD_0") != p.attributes.end()) {
					const tinygltf::Accessor& accessor = gltfModel.accessors[p.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// append data to vertices
				for (size_t v = 0; v < vertexCount; v++) {
					Vertex newvtx;
					newvtx.position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					newvtx.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) :
						glm::vec3(0.0f)));
					glm::vec2 uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec2(0.0f);
					newvtx.uv_x = uv.x;
					newvtx.uv_y = uv.y;
					newvtx.color = glm::vec4(1.0f);
					vertices.push_back(newvtx);
				}
			}

			// indices
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[p.indices];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
				indices.reserve(indices.size() + accessor.count);

				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t i = 0; i < accessor.count; i++) {
						indices.push_back(buf[i] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t i = 0; i < accessor.count; i++) {
						indices.push_back(buf[i] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t i = 0; i < accessor.count; i++) {
						indices.push_back(buf[i] + vertexStart);
					}
					break;
				}
				default:
					ENGINE_ERROR("Index component type unrecognized");
				}
			}
			newmesh.surfaces.push_back(newSurface);
		}

		constexpr bool OverrideColors = true;
		if (OverrideColors) {
			for (Vertex& vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.f);
			}
		}
		newmesh.meshBuffers = engine->upload_mesh(indices, vertices);
		fprintf(stderr, "[GLTF Loader] Found %d vertices.\n", vertices.size());
		fprintf(stderr, "[GLTF Loader] Created %d surfaces.\n",
			newmesh.surfaces.size());

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
	}

	fprintf(stderr, "Found %d meshes\n", mesh_count);
	return meshes;
}
