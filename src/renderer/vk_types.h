#ifndef VK_TYPES_H
#define VK_TYPES_H

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#include "vk_dispatch.h"
#include "glm/glm.hpp"

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

struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct Vertex {
	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

struct GPUMeshBuffers {
	VkDeviceSize	indexOffset;
	VkDeviceAddress vertexBufferAddress;
};

struct GeoSurface {
	uint32_t startIndex;
	uint32_t count;
};

struct MeshAsset {
	std::string name;
	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

struct GPUDrawPushConstants {
	glm::mat4 worldMatrix;
	VkDeviceAddress vertexBuffer;
};

#endif /* VK_TYPES_H */