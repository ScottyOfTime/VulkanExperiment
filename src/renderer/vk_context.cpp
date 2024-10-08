#include "vk_context.h"

#include <vector>

#include "vk_text.h"

void
DrawContext::add_line(glm::vec3 from, glm::vec3 to, glm::vec4 color) {
	_lineData.push_back(LineVertex{
		.color = color,
		.position = from
	});
	_lineData.push_back(LineVertex{
		.color = color,
		.position = to
	});
}

void 
DrawContext::add_triangle(glm::vec3 vertices[3], glm::vec4 color) {
	for (size_t i = 0; i < 3; i++) {
		_triangleData.push_back(TriangleVertex{
			.color = color,
			.position = vertices[i]
		});
	}
 }

void
DrawContext::add_mesh(const MeshAsset* mesh, const Transform* transform) {
	glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), transform->position);
	modelMatrix *= glm::mat4_cast(transform->rotation);
	modelMatrix = glm::scale(modelMatrix, transform->scale);

	for (size_t i = 0; i < mesh->surfaces.size(); i++) {
		const GeoSurface* surface = &mesh->surfaces[i];
		SurfaceDrawData data;
		data.indexCount = surface->count;
		data.firstIndex = surface->startIndex;
		data.indexBufferAddr = mesh->indexOffset;
		data.materialID = surface->materialID;
		data.vertexBufferAddr = mesh->vertexOffset;
		data.transform = modelMatrix;

		_surfaceData.push_back(data);
	}
}

void
DrawContext::add_text(const char* text, glm::vec3 position, FontAtlas* pAtlas) {
	uint32_t vertexOffset = _textData.vertices.size();
	float offsetX = position.x, offsetY = position.y;
	for (size_t i = 0; text[i] != '\0'; i++) {
		char c = text[i];
		if (c < FIRST_CHAR || c > LAST_CHAR) {
			continue;
		}

		GlyphInfo info = pAtlas->get_glyph_info(c, offsetX, offsetY);
		for (size_t i = 0; i < 4; i++) {
			_textData.vertices.push_back(TextVertex{
				.position = info.positions[i],
				.uv = info.uvs[i],
				// make all text white temporarily
				.color = { 1.f, 1.f, 1.f },
				.fontDescriptorIndex = pAtlas->descriptorIndex
			});
		}

		_textData.indices.push_back(vertexOffset + 0);
		_textData.indices.push_back(vertexOffset + 1);
		_textData.indices.push_back(vertexOffset + 2);
		_textData.indices.push_back(vertexOffset + 0);
		_textData.indices.push_back(vertexOffset + 2);
		_textData.indices.push_back(vertexOffset + 3);

		vertexOffset += 4;
	}
}

// This function is a bit iffy because there is no check or enforcment
// of the structure of data in vertices... but we assume that they come
// in as a list of triangles.
// TODO -> THIS FUNCTION IS NOT IMPLEMENTED DO NOT USE
void
DrawContext::add_wireframe(std::vector<glm::vec3>& vertices) {
	uint32_t vertexCount = 0, vertexOffset = _wireframeData.vertices.size();
	uint32_t triangleIndices[6] = { 0, 1, 2, 0 ,2, 3 };
	for (size_t i = 0; i < vertices.size(); i++) {
		_wireframeData.vertices.push_back(Vertex{
			.position = vertices[i],
			.color = glm::vec4(1, 0, 1, 1)
		});
		vertexCount++;
	}
}

void
DrawContext::clear() {
	_lineData.clear();
	_triangleData.clear();
	_surfaceData.clear();
	_textData.vertices.clear();
	_textData.indices.clear();
	_wireframeData.vertices.clear();
	_wireframeData.indices.clear();
}