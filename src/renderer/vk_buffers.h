#ifndef VK_BUFFERS_H
#define VK_BUFFERS_H

#include "vk_types.h"

struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocator allocator;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct BufferCreateInfo {
	VmaAllocator				allocator;

	AllocatedBuffer*			pBuffer;
	size_t						allocSize;
	VkBufferUsageFlags			usage;
	VmaAllocationCreateFlags	flags;
};

void create_buffer(BufferCreateInfo* create_info);


struct CopyDataToBufferInfo {
	VkBuffer		buffer;
	void*			pData;
	size_t			size;

	VkDeviceSize	srcOffset = 0;
	VkDeviceSize	dstOffset = 0;

	// By default use same allocator as the target buffer
	VmaAllocator	allocator;

	VkDevice		device;
	DeviceDispatch*	pDeviceDispatch;
	VkCommandBuffer	cmdBuf;
	VkFence			cmdFence;
	VkQueue			queue;
};

void copy_data_to_buffer(CopyDataToBufferInfo* copyInfo);

void destroy_buffer(const AllocatedBuffer* buffer);

#endif /* VK_BUFFERS_H */