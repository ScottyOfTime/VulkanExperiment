#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include "vk_types.h"
#include "vk_text.h"

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

	void						add_line(glm::vec3 from, glm::vec3 to, glm::vec4 color);
	void						add_triangle(glm::vec3 vertices[3], glm::vec4 color);
	void						add_mesh(const MeshAsset* mesh, const Transform* transform);
	void						add_text(const char* text, glm::vec3 position, FontAtlas* pAtlas);
	void						add_wireframe(std::vector<glm::vec3>& vertices);

	void						clear();

//private: the data below SHOULD be private but I want to access it directly until
	// the resot of the rendering logic is move here
	std::vector<LineVertex>		_lineData = {};
	std::vector<TriangleVertex>	_triangleData = {};
	std::vector<SurfaceDrawData>_surfaceData = {};
	TextDrawDataS				_textData = {};
	WireframeDrawDataS			_wireframeData = {};
};

#endif /* VK_CONTEXT_H */