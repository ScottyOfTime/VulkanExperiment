#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include "vk_types.h"
#include "vk_buffers.h"
#include "vk_text.h"

// These are normalized coordinates (i.e 0 to 1)
struct ShadowAtlasRegion {
	glm::vec2 offset;
	glm::vec2 scale;
};

/*
* This class is the main data structure that provides a "global" view
* of a scene in a frame.
*/

class DrawContext {
public:
	enum class DrawType { 
		LINE,
		TRIANGLE,
		MESH,
		TEXT,
		WIREFRAME
	};

	void							init(uint32_t shadowAtlasExtent, uint32_t numSupportedLights);

	void							add_line(glm::vec3 from, glm::vec3 to, glm::vec4 color);
	void							add_triangle(glm::vec3 vertices[3], glm::vec4 color);
	void							add_mesh(const Mesh* mesh, const Transform* transform);
	void							add_text(const char* text, glm::vec3 position, FontAtlas* pAtlas);
	void							add_wireframe(std::vector<glm::vec3>& vertices);

	void							add_light(const Light* light);

	void							clear();

//private: the data below SHOULD be private but I want to access it directly until
	// the rest of the rendering logic is moved here
	std::vector<LineVertex>			_lineData = {};
	std::vector<TriangleVertex>		_triangleData = {};
	std::vector<SurfaceDrawData>	_surfaceData = {};
	TextDrawDataS					_textData = {};
	WireframeDrawDataS				_wireframeData = {};

	uint32_t						_numSupportedLights;
	std::vector<Light>				_lights = {};
	std::vector<ShadowAtlasRegion>	_shadowAtlasRegions = {};
};

#endif /* VK_CONTEXT_H */