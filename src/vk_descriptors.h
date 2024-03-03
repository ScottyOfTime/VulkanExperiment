#include <vector>
#include <span>
#include <vulkan/vulkan.h>

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
		DeviceDispatch *deviceDispatch);
	void clear_descriptors(VkDevice device, DeviceDispatch *deviceDispatch);
	void destroy_pool(VkDevice device, DeviceDispatch *deviceDispatch);

	VkDescriptorSet alloc(VkDevice device, VkDescriptorSetLayout layout,
		DeviceDispatch *deviceDispatch);
};