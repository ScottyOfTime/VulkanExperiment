#include "vk_images.h"

void transition_image(VkCommandBuffer cmdBuf, VkImage image, VkImageLayout currentLayout,
	VkImageLayout newLayout, DeviceDispatch *deviceDispatch) {
	VkImageMemoryBarrier2 imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.pNext = NULL;
	
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;

	VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ?
		VK_IMAGE_ASPECT_DEPTH_BIT :
		VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = aspectMask;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	imageBarrier.subresourceRange = subresourceRange;
	imageBarrier.image = image;

	VkDependencyInfo di = {};
	di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	di.pNext = NULL;

	di.imageMemoryBarrierCount = 1;
	di.pImageMemoryBarriers = &imageBarrier;

	deviceDispatch->vkCmdPipelineBarrier2(cmdBuf, &di);
}
