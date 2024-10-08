#include "vk_buffers.h"

#include <vk_mem_alloc.h>

void create_buffer(BufferCreateInfo* createInfo) {
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = NULL;
	bufferInfo.size = createInfo->allocSize;
	bufferInfo.usage = createInfo->usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	vmaallocInfo.flags = createInfo->flags;

	createInfo->pBuffer->allocator = createInfo->allocator;

	vmaCreateBuffer(
		createInfo->allocator,
		&bufferInfo,
		&vmaallocInfo,
		&createInfo->pBuffer->buffer,
		&createInfo->pBuffer->allocation,
		&createInfo->pBuffer->info
	);
}

void copy_data_to_buffer(CopyDataToBufferInfo* copyInfo) {
	VkResult res = {};

	AllocatedBuffer uploadBuffer;
	BufferCreateInfo bufferInfo;
	bufferInfo.allocator = copyInfo->allocator;
	bufferInfo.pBuffer = &uploadBuffer;
	bufferInfo.allocSize = copyInfo->size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	create_buffer(&bufferInfo);

	memcpy(uploadBuffer.info.pMappedData, copyInfo->pData, copyInfo->size);

	res = copyInfo->pDeviceDispatch->vkResetFences(copyInfo->device, 1,
		&copyInfo->cmdFence);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_buffer] Failed to reset command fence.\n");
	}
	res = copyInfo->pDeviceDispatch->vkResetCommandBuffer(copyInfo->cmdBuf, 0);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_buffer] Failed to reset command buffer.\n");
	}

	VkCommandBufferBeginInfo cmdBi = {};
	cmdBi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBi.pNext = NULL;
	cmdBi.pInheritanceInfo = NULL;
	cmdBi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	res = copyInfo->pDeviceDispatch->vkBeginCommandBuffer(copyInfo->cmdBuf, &cmdBi);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_buffer] Failed to begin command buffer.\n");
	}

	VkBufferCopy bufferCopy{};
	bufferCopy.srcOffset = copyInfo->srcOffset;
	bufferCopy.dstOffset = copyInfo->dstOffset;
	bufferCopy.size = copyInfo->size;

	copyInfo->pDeviceDispatch->vkCmdCopyBuffer(copyInfo->cmdBuf, uploadBuffer.buffer,
		copyInfo->buffer, 1, &bufferCopy);

	res = copyInfo->pDeviceDispatch->vkEndCommandBuffer(copyInfo->cmdBuf);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_buffer] Failed to end command buffer.\n");
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
		fprintf(stderr, "[copy_data_to_buffer] Failed to submit command buffer.\n");
	}

	res = copyInfo->pDeviceDispatch->vkWaitForFences(copyInfo->device, 1,
		&copyInfo->cmdFence, true, 9999999999);
	if (res != VK_SUCCESS) {
		fprintf(stderr, "[copy_data_to_buffer] Failed to wait for command fence. %d\n", res);
	}

	destroy_buffer(&uploadBuffer);
}

void destroy_buffer(const AllocatedBuffer* buffer) {
	vmaDestroyBuffer(buffer->allocator, buffer->buffer, buffer->allocation);
}