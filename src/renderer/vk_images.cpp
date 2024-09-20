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

void copy_image_to_image(VkCommandBuffer cmdBuf, VkImage srcImg, VkImage dstImg, 
	VkExtent2D srcSize, VkExtent2D dstSize, DeviceDispatch* deviceDispatch) {
	VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

	blitRegion.srcOffsets[1].x = srcSize.width;
	blitRegion.srcOffsets[1].y = srcSize.height;
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = dstSize.width;
	blitRegion.dstOffsets[1].y = dstSize.height;
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blitInfo.dstImage = dstImg;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = srcImg;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	deviceDispatch->vkCmdBlitImage2(cmdBuf, &blitInfo);
}