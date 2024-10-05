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

	vmaCreateBuffer(
		createInfo->allocator,
		&bufferInfo,
		&vmaallocInfo,
		&createInfo->pBuffer->buffer,
		&createInfo->pBuffer->allocation,
		&createInfo->pBuffer->info
	);
}

void destroy_buffer(VmaAllocator allocator, const AllocatedBuffer* buffer) {
	vmaDestroyBuffer(allocator, buffer->buffer, buffer->allocation);
}