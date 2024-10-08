#ifndef VK_TYPES_H
#define VK_TYPES_H

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#include "vk_dispatch.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdint.h>
#include <string>

/*---------------------------
|  FLAGS AND CONSTANTS
---------------------------*/

// If these go above 64 we get very close to UBO limit
#define MAX_POINT_LIGHTS	64
#define MAX_DIR_LIGHTS		64
#define MAX_SPOT_LIGHTS		64

typedef uint32_t			DebugFlags;

enum DebugFlagBits : uint32_t {
	DEBUG_FLAGS_ENABLE = 1 << 0,
	DEBUG_FLAGS_GEOMETRY_WIREFRAME = 1 << 1
};

/*---------------------------
|  MISC (YET TO BE ORGANIZED)
---------------------------*/

struct AllocatedImage {
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct ImguiLoaderData {
	VkInstance instance;
	InstanceDispatch* instanceDispatch;
};

struct GeoSurface {
	uint32_t startIndex;
	uint32_t count;
	uint32_t materialID;
};

struct MeshAsset {
	std::string 			name;
	std::vector<GeoSurface> surfaces;
	VkDeviceSize			indexOffset;		
	VkDeviceSize			vertexOffset;
};

// Holy padding - fuckin fix this
struct Material {
	glm::vec3 ambient;
	char padding1[4];
	glm::vec3 diffuse;
	char padding2[4];
	glm::vec3 specular;
	float shininess;
};

/*---------------------------
|  VERTEX TYPES (FOR DRAWING DIFFERENT SHAPES/PRIMITIVES)
---------------------------*/

// This struct is used for textured mesh triangles with normals
struct Vertex {
	glm::vec3	position;
	float		uv_x;
	glm::vec3	normal;
	float		uv_y;
	glm::vec4	color;
};

// This struct is used for colored triangles with no normal info
struct TriangleVertex {
	glm::vec4	color;
	glm::vec3	position;
	char		padding[4];
};

// This struct is used for text
struct TextVertex {
	glm::vec3	position;
	char		padding1[4];
	glm::vec2	uv;
	char		padding2[8];
	glm::vec3	color;
	uint32_t	fontDescriptorIndex;
};

// This struct is used for simple line drawing
struct LineVertex {
	glm::vec4	color;
	glm::vec3	position;
	char		padding[4];
};

/*---------------------------
|  INTERFACE TYPES (FOR USE BOTH INSIDE AND OUTSIDE OF RENDERER)
---------------------------*/

struct Transform {
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;
};

struct Light {
	glm::vec3 position;
	char padding1[4];
	glm::vec3 ambient;
	char padding2[4];
	glm::vec3 diffuse;
	char padding3[4];
	glm::vec3 specular;
};

struct DirectionalLight {
	glm::vec3	direction;
	char padding1[4];
	glm::vec3	ambient;
	char padding2[4];
	glm::vec3	diffuse;
	char padding3[4];
	glm::vec3	specular;
};

struct PointLight {
	glm::vec3	position;
	char padding1[4];
	glm::vec3	ambient;
	char padding2[4];
	glm::vec3	diffuse;
	char padding3[4];
	glm::vec3	specular;
	float		constant;
	float		linear;
	float		quadratic;
};

struct SpotLight {
	glm::vec3	position;
	char padding1[4];
	glm::vec3	direction;
	char padding2[4];
	glm::vec3	ambient;
	char padding3[4];
	glm::vec3	diffuse;
	char padding4[4];
	glm::vec3	specular;
	float		constant;
	float		linear;
	float		quadratic;
	float		cutOff;
	float		outerCutOff;
};

struct SurfaceDrawData {
	uint32_t		indexCount;
	uint32_t		firstIndex;
	VkDeviceAddress indexBufferAddr;

	uint32_t		materialID;

	glm::mat4		transform;
	VkDeviceAddress vertexBufferAddr;
};

struct TextDrawDataS {
	std::vector<TextVertex> vertices;
	std::vector<uint32_t>	indices;
};

struct WireframeDrawDataS {
	std::vector<Vertex>		vertices;
	std::vector<uint32_t>	indices;
};

/*---------------------------
|  GPU STRUCTURES (USED PRIMARILY BY RENDERER TO SEND TO GPU)
---------------------------*/

struct GPUSceneData {
	glm::mat4		view;
	glm::mat4		proj;
	glm::mat4		orthoProj;
};

struct GPULightData {
	VkDeviceAddress directionalLights;
	VkDeviceAddress pointLights;
	VkDeviceAddress spotLights;

	uint32_t numPointLights = 0;
	uint32_t numDirectionalLights = 0;
	uint32_t numSpotLights = 0;
};

struct GPUDrawPushConstants {
	VkDeviceAddress sceneBuffer;
	VkDeviceAddress vertexBuffer;
	VkDeviceAddress	materialBuffer;
	VkDeviceAddress lightBuffer;
	uint32_t		materialID;
	// Hopefully something will fill this padding
	char			padding[12];
	glm::mat4		model;
	glm::vec3		viewPos;
};


#endif /* VK_TYPES_H */
