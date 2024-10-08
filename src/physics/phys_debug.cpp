#include "phys_debug.h"

void DebugRendererSimpleImpl::DrawLine(RVec3Arg inFrom, RVec3Arg inTo, 
	ColorArg inColor) {

	glm::vec3 from = glm::vec3(
		inFrom.GetX(),
		inFrom.GetY(),
		inFrom.GetZ()
	);
	glm::vec3 to = glm::vec3(
		inTo.GetX(),
		inTo.GetY(),
		inTo.GetZ()
	);
	glm::vec4 color = glm::vec4(
		inColor.r,
		inColor.b,
		inColor.g,
		inColor.a
	);

	_pVulkanEngine->draw_line(from, to, color);
}

void DebugRendererSimpleImpl::DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3,
	ColorArg inColor, ECastShadow inCastShadow) {

	glm::vec3 v1 = glm::vec3(
		inV1.GetX(),
		inV1.GetY(),
		inV1.GetZ()
	);
	glm::vec3 v2 = glm::vec3(
		inV2.GetX(),
		inV2.GetY(),
		inV2.GetZ()
	);
	glm::vec3 v3 = glm::vec3(
		inV3.GetX(),
		inV3.GetY(),
		inV3.GetZ()
	);
	glm::vec4 color = glm::vec4(
		inColor.r,
		inColor.g,
		inColor.b,
		inColor.a
	);

	glm::vec3 vertices[3] = { v1, v2, v3 };
	_pVulkanEngine->draw_triangle(vertices, color);
}

void DebugRendererSimpleImpl::DrawText3D(RVec3Arg inPosition, 
	const string_view& inString, ColorArg inColor, float inHeight) {
	return;
}