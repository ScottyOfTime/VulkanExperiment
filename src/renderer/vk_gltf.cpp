#include "vk_gltf.h"

#include "vk_engine.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

void load_gltf_meshes(VulkanEngine* pVulkanEngine, const char* filename) {
	// Only assumes one node in the gLTF file, let's enforce this
	tinygltf::TinyGLTF context;
	tinygltf::Model model;

	std::string err;
	std::string warn;

	bool res = context.LoadBinaryFromFile(&model, &err, &warn, filename);
	if (!warn.empty()) {
		fprintf(stderr, "[gLTF Loader] Warning: %s\n", warn.c_str());
	}
	if (!err.empty()) {
		fprintf(stderr, "[gLTF Loader] Error: %s\n", err.c_str());
	}

	if (!res) {
		fprintf(stderr, "[gLTF Loader] Failed to load gLTF\n");
		return;
	}
	else {
		fprintf(stderr, "[gLTF Loader] Loaded gLTF file: %s\n", filename);
	}

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	for (size_t i = 0; i < model.meshes.size(); i++) {
		const tinygltf::Mesh* mesh = &model.meshes[i];

		Mesh newMesh;
		newMesh.name = mesh->name;

		indices.clear();
		vertices.clear();

		// iterate through all the primitives (what we call surfaces)
		for (size_t i = 0; i < mesh->primitives.size(); i++) {
			const tinygltf::Primitive* p = &mesh->primitives[i];
			Surface newSurface;
			newSurface.startIndex = static_cast<uint32_t>(indices.size());
			newSurface.count = static_cast<uint32_t>(model.accessors[p->indices].count);
			uint32_t vertexStart = static_cast<uint32_t>(vertices.size());
			uint32_t indexCount = 0;

			// Load vertex data
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				uint32_t vertexCount = 0;

				// get the buffer for vertex positions
				if (p->attributes.find("POSITION") != p->attributes.end()) {
					const tinygltf::Accessor* accessor = &model.accessors[p->attributes.find("POSITION")->second];
					const tinygltf::BufferView* view = &model.bufferViews[accessor->bufferView];
					const tinygltf::Buffer* buffer = &model.buffers[view->buffer];
					positionBuffer = reinterpret_cast<const float*>(&buffer->data[accessor->byteOffset + view->byteOffset]);
					vertexCount = accessor->count;

				}

				// get the buffer for vertex normals
				if (p->attributes.find("NORMAL") != p->attributes.end()) {
					const tinygltf::Accessor* accessor = &model.accessors[p->attributes.find("NORMAL")->second];
					const tinygltf::BufferView* view = &model.bufferViews[accessor->bufferView];
					const tinygltf::Buffer* buffer = &model.buffers[view->buffer];
					normalsBuffer = reinterpret_cast<const float*>(&buffer->data[accessor->byteOffset + view->byteOffset]);
				}

				// get the buffer for vertex texture coordinates
				if (p->attributes.find("TEXCOORD_0") != p->attributes.end()) {
					const tinygltf::Accessor* accessor = &model.accessors[p->attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView* view = &model.bufferViews[accessor->bufferView];
					const tinygltf::Buffer* buffer = &model.buffers[view->buffer];
					texCoordsBuffer = reinterpret_cast<const float*>(&buffer->data[accessor->byteOffset + view->byteOffset]);
				}

				if (positionBuffer == nullptr) {
					fprintf(stderr, "[gLTF Loader] Primitive missing vertex data");
					return;
				}

				// append data to vertices
				for (size_t v = 0; v < vertexCount; v++) {
					Vertex newVertex;
					newVertex.position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					newVertex.normal = glm::normalize(
						glm::vec3(normalsBuffer ?
							glm::make_vec3(&normalsBuffer[v * 3]) :
							glm::vec3(0.0f)
						)
					);
					glm::vec2 uv = texCoordsBuffer ?
						glm::make_vec2(&texCoordsBuffer[v * 2]) :
						glm::vec2(0.0f);
					newVertex.uv_x = uv.x;
					newVertex.uv_y = uv.y;
					newVertex.color = glm::vec4(1.0f);
					vertices.push_back(newVertex);
				}
			}

			// Load index data
			{
				const tinygltf::Accessor* accessor = &model.accessors[p->indices];
				const tinygltf::BufferView* bufferView = &model.bufferViews[accessor->bufferView];
				const tinygltf::Buffer* buffer = &model.buffers[bufferView->buffer];

				switch (accessor->componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer->data[accessor->byteOffset + bufferView->byteOffset]);
					for (size_t i = 0; i < accessor->count; i++) {
						indices.push_back(buf[i] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer->data[accessor->byteOffset + bufferView->byteOffset]);
					for (size_t i = 0; i < accessor->count; i++) {
						indices.push_back(buf[i] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer->data[accessor->byteOffset + bufferView->byteOffset]);
					for (size_t i = 0; i < accessor->count; i++) {
						indices.push_back(buf[i] + vertexStart);
					}
					break;
				}
				default:
					fprintf(stderr, "[gLTF Loader] Index component type unrecognized.\n");
					return;
				}
			}
			newMesh.surfaces.push_back(newSurface);
		}
		UploadMeshInfo info = {};
		info.pVertices = vertices.data();
		info.vertexCount = vertices.size();
		info.pIndices = indices.data();
		info.indexCount = indices.size();
		info.pSurfaces = newMesh.surfaces.data();
		info.surfaceCount = newMesh.surfaces.size();
		pVulkanEngine->upload_mesh(&info);
	}
}