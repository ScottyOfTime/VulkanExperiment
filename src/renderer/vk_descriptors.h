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
	std::vector<VkDescriptorBindingFlags> bindingFlags;

	void add_binding(uint32_t binding, VkDescriptorType type, uint32_t count,
		VkDescriptorBindingFlags flags);
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

struct DescriptorWriter {
public:
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	uint32_t write_image(int binding, VkImageView image, VkSampler sampler,
		VkImageLayout layout, VkDescriptorType type);
	uint32_t write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
		VkDescriptorType type);

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set, DeviceDispatch* deviceDispatch);
private:
	uint32_t imgIndex = 0;
	uint32_t bufIndex = 0;
};