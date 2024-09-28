#include "vk_suballocator.h"

#include <stdio.h>

uint32_t VkBufferSuballocator::create_buffer(VkDevice device, VmaAllocator allocator, 
			VkDeviceSize allocSize, VkBufferUsageFlags usage,
			VmaAllocationCreateFlags vmaFlags, DeviceDispatch* deviceDispatch) {
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	// @Review -> 	Not sure if these flags allow for the fastest possible usage
	// 				on GPU
	allocInfo.flags = vmaFlags;

	if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation,
			&info) != VK_SUCCESS) {
		fprintf(stderr, "[Suballocator] Failed to create buffer.\n");
		return 1;
	}
	size = allocSize;
	offset = 0;

	VkBufferDeviceAddressInfo addrInfo = {};
	addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addrInfo.buffer = buffer;
	addr = deviceDispatch->vkGetBufferDeviceAddress(device, &addrInfo);

	fprintf(stderr, "[Suballocator] Successfully created buffer with size %d at "
			"address %x.\n", allocSize, addr);

	return 0;
}

size_t VkBufferSuballocator::suballocate(VkDeviceSize allocSize, VkDeviceSize alignment) {
	if (buffer == VK_NULL_HANDLE) {
		fprintf(stderr, "[Suballocator] Buffer allocator not yet initialized.\n");
		return 0;
	}

	// Calculate offset accounting for alignment requirements	
	VkDeviceSize alignedOffset = (offset + alignment - 1) & ~(alignment - 1);

	// Check if theres enough space for suballocation
	if (alignedOffset + allocSize > size) {
		fprintf(stderr, "[Suballocator] Suballocator out of memory.\n");
		return VK_WHOLE_SIZE;
	}
	offset = alignedOffset + allocSize;
	return alignedOffset;
}

void VkBufferSuballocator::reset() {
	offset = 0;
}

void VkBufferSuballocator::destroy_buffer(VmaAllocator allocator) {
	if (buffer == VK_NULL_HANDLE) {
		fprintf(stderr, "[Allocator] Buffer allocator not yet initialized.\n");
		return;
	}

	vmaDestroyBuffer(allocator, buffer, allocation);
}
