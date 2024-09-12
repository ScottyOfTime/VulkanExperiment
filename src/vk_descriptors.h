#include <vector>
#include <span>
#include <deque>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <cstdlib>

#include "vk_dispatch.h"

#define DESCRIPTOR_ERROR(MSG) \
	fprintf(stderr, "[VulkanDescriptors] ERROR: " MSG "\n");

struct DescriptorLayoutBuilder {
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void add_binding(uint32_t binding, VkDescriptorType type);
	void clear();
	VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages,
		DeviceDispatch *deviceDispatch);
};

struct DescriptorAllocator {
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	VkDescriptorPool pool;

	void init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios,
		DeviceDispatch* deviceDispatch);
	void clear_descriptors(VkDevice device, DeviceDispatch* deviceDispatch);
	void destroy_pool(VkDevice device, DeviceDispatch* deviceDispatch);

	VkDescriptorSet alloc(VkDevice device, VkDescriptorSetLayout layout,
		DeviceDispatch* deviceDispatch);
};

struct DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios,
		DeviceDispatch* deviceDispatch);
	void clear_pools(VkDevice device, DeviceDispatch* deviceDispatch);
	void destroy_pools(VkDevice device, DeviceDispatch* deviceDispatch);

	VkDescriptorSet alloc(VkDevice device, VkDescriptorSetLayout layout,
		DeviceDispatch* deviceDispatch, void* pNext = nullptr);
private:
	VkDescriptorPool get_pool(VkDevice device, DeviceDispatch* deviceDispatch);
	VkDescriptorPool create_pool(VkDevice device, uint32_t setCount,
		std::span<PoolSizeRatio> poolRatios, DeviceDispatch* deviceDispatch);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;
};

struct DescriptorWriter {
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	void write_image(int binding, VkImageView image, VkSampler sampler,
		VkImageLayout layout, VkDescriptorType type);
	void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
		VkDescriptorType type);

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set, DeviceDispatch* deviceDispatch);
};