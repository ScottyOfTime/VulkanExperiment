#include "vk_images.h"

#include "vk_buffers.h"

#include <stb_image.h>

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

void copy_data_to_image(CopyDataToImageInfo* copyInfo) {
	VkResult res = {};

	AllocatedBuffer uploadBuffer;
	BufferCreateInfo bufferInfo;
	bufferInfo.allocator = copyInfo->allocator;
	bufferInfo.pBuffer = &uploadBuffer;
	bufferInfo.allocSize = copyInfo->data_size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	create_buffer(&bufferInfo);

	memcpy(uploadBuffer.info.pMappedData, copyInfo->pData, copyInfo->data_size);

	res = copyInfo->pDeviceDispatch->vkResetFences(copyInfo->device, 1, 
		&copyInfo->cmdFence);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_image] Failed to reset command fence.\n");
	}
	res = copyInfo->pDeviceDispatch->vkResetCommandBuffer(copyInfo->cmdBuf, 0);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_image] Failed to reset command buffer.\n");
	}

	VkCommandBufferBeginInfo cmdBi = {};
	cmdBi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBi.pNext = NULL;
	cmdBi.pInheritanceInfo = NULL;
	cmdBi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	res = copyInfo->pDeviceDispatch->vkBeginCommandBuffer(copyInfo->cmdBuf, &cmdBi);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_image] Failed to begin command buffer.\n");
	}

	transition_image(copyInfo->cmdBuf, copyInfo->dstImg, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyInfo->pDeviceDispatch);

	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;

	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = copyInfo->arrayIndex;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent = copyInfo->extent;

	copyInfo->pDeviceDispatch->vkCmdCopyBufferToImage(
		copyInfo->cmdBuf, uploadBuffer.buffer, copyInfo->dstImg,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion
	);

	transition_image(copyInfo->cmdBuf, copyInfo->dstImg,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, copyInfo->pDeviceDispatch);

	res = copyInfo->pDeviceDispatch->vkEndCommandBuffer(copyInfo->cmdBuf);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_image] Failed to end command buffer.\n");
	}

	VkCommandBufferSubmitInfo cmdSi = {};
	cmdSi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdSi.pNext = NULL;
	cmdSi.commandBuffer = copyInfo->cmdBuf;
	cmdSi.deviceMask = 0;

	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreInfoCount = 0;
	submitInfo.pWaitSemaphoreInfos = NULL;
	submitInfo.signalSemaphoreInfoCount = 0;
	submitInfo.pSignalSemaphoreInfos = NULL;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdSi;

	res = copyInfo->pDeviceDispatch->vkQueueSubmit2(copyInfo->queue, 1,
		&submitInfo, copyInfo->cmdFence);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_image] Failed to submit command buffer.\n");
	}

	res = copyInfo->pDeviceDispatch->vkWaitForFences(copyInfo->device, 1,
		&copyInfo->cmdFence, true, 9999999999);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_image] Failed to wait for command fence.\n");
	}

	destroy_buffer(&uploadBuffer);
}

void create_image(ImageCreateInfo* createInfo) {
	uint32_t res;

	createInfo->pImg->imageFormat = createInfo->format;
	createInfo->pImg->imageExtent = createInfo->size;

	VkImageCreateInfo imgInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.pNext = nullptr;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = createInfo->format;
	imgInfo.extent = createInfo->size;
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = createInfo->usage;

	if (createInfo->mipmapped) {
		imgInfo.mipLevels = static_cast<uint32_t>(std::floor(
			std::log2(std::max(createInfo->size.width, createInfo->size.height))
		)) + 1;
	}

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	res = vmaCreateImage(createInfo->allocator, &imgInfo, &allocInfo,
		&createInfo->pImg->image, &createInfo->pImg->allocation, nullptr);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[Images] VMA failed to create image.\n");
	}

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	if (createInfo->format == VK_FORMAT_D32_SFLOAT) {
		aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build an image-view for the image
	VkImageViewCreateInfo viewInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.pNext = nullptr;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.image = createInfo->pImg->image;
	viewInfo.format = createInfo->format;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = imgInfo.mipLevels;
	viewInfo.subresourceRange.aspectMask = aspectFlags;

	createInfo->pDeviceDispatch->vkCreateImageView(
		createInfo->device,
		&viewInfo,
		nullptr,
		&createInfo->pImg->imageView
	);
}

void create_image_with_data(ImageCreateInfo* createInfo, void* data,
	VkCommandBuffer cmd, VkFence fence, VkQueue queue) {
	uint32_t res = 0, stride = 0;

	switch (createInfo->format) {
		case VK_FORMAT_R8_UNORM:
			stride = 1;
			break;
		case VK_FORMAT_R8G8B8A8_UNORM:
			stride = 4;
			break;
		default:
			fprintf(stderr, "[Images] Unrecognised image format defaulting to rgba_unorm.\n");
			break;
	}
	uint32_t data_size = createInfo->size.depth * createInfo->size.width 
		* createInfo->size.height * stride;

	AllocatedBuffer uploadBuffer;
	BufferCreateInfo bufferInfo;
	bufferInfo.allocator = createInfo->allocator;
	bufferInfo.pBuffer = &uploadBuffer;
	bufferInfo.allocSize = data_size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	create_buffer(&bufferInfo);

	memcpy(uploadBuffer.info.pMappedData, data, data_size);

	ImageCreateInfo imageInfo = *createInfo;
	imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	create_image(&imageInfo);

	// Record the provided command buffer the copy instructions
	CopyDataToImageInfo copyInfo;
	copyInfo.allocator = createInfo->allocator;
	copyInfo.cmdBuf = cmd;
	copyInfo.cmdFence = fence;
	copyInfo.pData = data;
	copyInfo.data_size = data_size;
	copyInfo.dstImg = createInfo->pImg->image;
	copyInfo.extent = createInfo->size;
	copyInfo.pDeviceDispatch = createInfo->pDeviceDispatch;
	copyInfo.device = createInfo->device;
	copyInfo.queue = queue;
	copy_data_to_image(&copyInfo);

	destroy_buffer(&uploadBuffer);
}

void create_image_cube_map(ImageCreateInfo* createInfo, const char* texturePrefix,
	VkCommandBuffer cmd, VkFence fence, VkQueue queue) {
	const char* files[] = {
		"right.png",
		"left.png",
		"top.png",
		"bottom.png",
		"front.png",
		"back.png"
	};

	stbi_uc* faceData[6];
	VkExtent3D faceExtents[6];

	int width, height, channels;
	for (size_t i = 0; i < 6; i++) {
		char full_path[256];
		strcpy(full_path, texturePrefix);
		strcat(full_path, files[i]);
		fprintf(stderr, "[Cubemap] Loading %s\n", full_path);
		stbi_uc* data = stbi_load(full_path, &width, &height, &channels,
			STBI_rgb_alpha);
		if (!data) {
			fprintf(stderr, "Failed to load cubemap image.");
		}
		faceData[i] = data;
		faceExtents[i] = VkExtent3D{
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height),
			.depth = 1
		};
	}

	VkImageCreateInfo imgInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.pNext = nullptr;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	// assuming all faces have the same extent which should be the case
	// for a cube map
	imgInfo.extent = faceExtents[0];
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 6;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	vmaCreateImage(createInfo->allocator, &imgInfo, &allocInfo,
		&createInfo->pImg->image, &createInfo->pImg->allocation, nullptr);

	// build an image-view for the image
	VkImageViewCreateInfo viewInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.pNext = nullptr;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewInfo.image = createInfo->pImg->image;
	viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 6;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	createInfo->pDeviceDispatch->vkCreateImageView(createInfo->device, &viewInfo, 
		nullptr, &createInfo->pImg->imageView);

	for (size_t i = 0; i < 6; i++) {
		size_t faceSize = faceExtents[i].width * faceExtents[i].height * 4;

		CopyDataToImageInfo copyInfo;
		copyInfo.device = createInfo->device;
		copyInfo.allocator = createInfo->allocator;
		copyInfo.cmdBuf = cmd;
		copyInfo.cmdFence = fence;
		copyInfo.queue = queue;
		copyInfo.pData = faceData[i];
		copyInfo.data_size = faceSize;
		copyInfo.dstImg = createInfo->pImg->image;
		copyInfo.extent = faceExtents[i];
		copyInfo.pDeviceDispatch = createInfo->pDeviceDispatch;
		copyInfo.arrayIndex = i;
		copy_data_to_image(&copyInfo);

		stbi_image_free(faceData[i]);
	}
}

void destroy_image(VkDevice device, DeviceDispatch* deviceDispatch,
	VmaAllocator allocator, const AllocatedImage* img) {
	deviceDispatch->vkDestroyImageView(device, img->imageView, nullptr);
	vmaDestroyImage(allocator, img->image, img->allocation);
}