#include "vk_context.h"

#include <vector>

#include "vk_text.h"

void
DrawContext::init(uint32_t shadowAtlasExtent, uint32_t numSupportedLights) {
	uint32_t shadowMapsPerRow = static_cast<uint32_t>(std::sqrt(numSupportedLights));
	uint32_t shadowMapGridSize = shadowAtlasExtent / shadowMapsPerRow;
	_numSupportedLights = numSupportedLights;

	_shadowAtlasRegions.resize(numSupportedLights);
	for (size_t i = 0; i < numSupportedLights; i++) {
		uint32_t x = i % shadowMapsPerRow;
		uint32_t y = i / shadowMapsPerRow;

		_shadowAtlasRegions[i].offset = glm::vec2(
			x * shadowMapGridSize,
			y * shadowMapGridSize
		) / glm::vec2(shadowAtlasExtent);
		_shadowAtlasRegions[i].scale = glm::vec2(shadowMapGridSize) / glm::vec2(shadowAtlasExtent);
	}
}

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
DrawContext::add_mesh(const Mesh* mesh, const Transform* transform) {
	glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), transform->position);
	modelMatrix *= glm::mat4_cast(transform->rotation);
	modelMatrix = glm::scale(modelMatrix, transform->scale);

	for (size_t i = 0; i < mesh->surfaces.size(); i++) {
		const Surface* surface = &mesh->surfaces[i];
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
		offsetX = info.offsetX;
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
DrawContext::add_light(const Light* light) {
	if (_lights.size() >= _numSupportedLights) {
		fprintf(stderr, "[DrawContext] Reached supported number of lights.\n");
		return;
	}

	Light newLight = *light;
	switch (light->type) {
	case LightType::Direction:
		float near_plane = 0.5f, far_plane = 20.0f;
		glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
		glm::vec3 origin = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 lightPosition = origin - light->direction * 15.f;
		newLight.position = lightPosition;

		glm::vec3 up = glm::abs(glm::dot(light->direction, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f ?
			glm::vec3(0.0f, 0.0f, 1.0f) :
			glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 lightView = glm::lookAt(
			lightPosition,
			origin,
			up
		);
		glm::mat4 lightSpaceMatrix = lightProjection * lightView;
		newLight.spaceMatrix = lightSpaceMatrix;
	}
	_lights.push_back(newLight);
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
	_lights.clear();
}