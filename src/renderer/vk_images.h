#ifndef VK_IMAGES_H
#define VK_IMAGES_H

#include <vulkan/vulkan.h>

#include "vk_dispatch.h"
#include "vk_types.h"

void transition_image(VkCommandBuffer cmdBuf, VkImage image, VkImageLayout currentLayout,
	VkImageLayout newLayout, DeviceDispatch *deviceDispatch);

void copy_image_to_image(VkCommandBuffer cmdBuf, VkImage srcImg, VkImage dstImg, VkExtent2D srcSize, 
	VkExtent2D dstSize, DeviceDispatch *deviceDispatch);

struct CopyDataToImageInfo {
	VmaAllocator	allocator;

	VkCommandBuffer cmdBuf;
	VkFence			cmdFence;
	VkQueue			queue;

	void*			pData;
	size_t			data_size;
	VkImage			dstImg;
	VkExtent3D		extent;

	VkDevice		device;
	DeviceDispatch* pDeviceDispatch;

	uint32_t		arrayIndex = 0;
};

void copy_data_to_image(CopyDataToImageInfo* copyInfo);

struct ImageCreateInfo {
	VmaAllocator		allocator;
	VkDevice			device;
	DeviceDispatch*		pDeviceDispatch;

	AllocatedImage*		pImg;
	VkExtent3D			size;
	VkFormat			format;
	VkImageUsageFlags	usage;
	uint8_t				mipmapped = 0;
};

void create_image(ImageCreateInfo* createInfo);

void create_image_with_data(ImageCreateInfo* createInfo, void* data,
	VkCommandBuffer cmd, VkFence fence, VkQueue queue);

void create_image_cube_map(ImageCreateInfo* createInfo, 
	const char* texturePrefix, VkCommandBuffer cmd, VkFence fence,
	VkQueue queue);

void destroy_image(VkDevice device, DeviceDispatch* deviceDisptach,
	VmaAllocator allocator, const AllocatedImage* img);

#endif /* VK_IMAGES_H */
