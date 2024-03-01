#ifndef VK_IMAGES_H
#define VK_IMAGES_H

#include <vulkan/vulkan.h>

#include "vk_dispatch.h"

void transition_image(VkCommandBuffer cmdBuf, VkImage image, VkImageLayout currentLayout,
	VkImageLayout newLayout, DeviceDispatch *deviceDispatch);

void copy_image_to_image(VkCommandBuffer cmdBuf, VkImage srcImg, VkImage dstImg, VkExtent2D srcSize, 
	VkExtent2D dstSize, DeviceDispatch *deviceDispatch);

#endif /* VK_IMAGES_H */
