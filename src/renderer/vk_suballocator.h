#ifndef VK_MEM_H
#define VK_MEM_H

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "vk_dispatch.h"

// This struct will create a VkBuffer via VMA then allow suballocations
// from created buffer.
class VkBufferSuballocator {
public:
	uint32_t 			create_buffer(VkDevice device, VmaAllocator allocator,
							VkDeviceSize allocSize, VkBufferUsageFlags usage,
							VmaAllocationCreateFlags vmaFlags,
							DeviceDispatch* deviceDispatch);
	size_t 				suballocate(VkDeviceSize allocSize, VkDeviceSize alignment);
	void				reset();

	void 				destroy_buffer(VmaAllocator allocator);

	VkBuffer 			buffer = VK_NULL_HANDLE;
	VkDeviceAddress		addr;
	VmaAllocation 		allocation;
	VmaAllocationInfo 	info;
private:
	VkDeviceSize 		size;
	VkDeviceSize		offset;
};


#endif /* VK_MEM_H */
