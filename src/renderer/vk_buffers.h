#ifndef VK_BUFFERS_H
#define VK_BUFFERS_H

#include "vk_types.h"

struct BufferCreateInfo {
	VmaAllocator				allocator;

	AllocatedBuffer*			pBuffer;
	size_t						allocSize;
	VkBufferUsageFlags			usage;
	VmaAllocationCreateFlags	flags;
};

void create_buffer(BufferCreateInfo* create_info);

void destroy_buffer(VmaAllocator allocator, const AllocatedBuffer* buffer);

#endif /* VK_BUFFERS_H */