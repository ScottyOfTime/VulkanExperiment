#include "vk_descriptors.h"

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) {
	VkDescriptorSetLayoutBinding newbind = {};
	newbind.binding = binding;
	newbind.descriptorCount = 1;
	newbind.descriptorType = type;

	bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages,
	DeviceDispatch* deviceDispatch) {
	for (auto& b : bindings) {
		b.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ci.pNext = NULL;
	ci.pBindings = bindings.data();
	ci.bindingCount = (uint32_t)bindings.size();
	ci.flags = 0;

	VkDescriptorSetLayout set;
	if (deviceDispatch->vkCreateDescriptorSetLayout(device, &ci, NULL, &set) != VK_SUCCESS) {
		DESCRIPTOR_ERROR("Failed to create descriptor set layout.");
		// REVIEW: This is yucky
		abort();
	}

	return set;
}

void DescriptorAllocator::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios,
	DeviceDispatch *deviceDispatch) {
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * maxSets)
		});
	}

	VkDescriptorPoolCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	ci.flags = 0;
	ci.maxSets = maxSets;
	ci.poolSizeCount = (uint32_t)poolSizes.size();
	ci.pPoolSizes = poolSizes.data();

	if (deviceDispatch->vkCreateDescriptorPool(device, &ci, NULL, &pool) != VK_SUCCESS) {
		DESCRIPTOR_ERROR("Failed to create descriptor pool.");
		// REVIEW: Again, yucky
		abort();
	}
}

void DescriptorAllocator::clear_descriptors(VkDevice device, DeviceDispatch *deviceDispatch) {
	deviceDispatch->vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device, DeviceDispatch *deviceDispatch) {
	deviceDispatch->vkDestroyDescriptorPool(device, pool, NULL);
}

VkDescriptorSet DescriptorAllocator::alloc(VkDevice device, VkDescriptorSetLayout layout,
	DeviceDispatch* deviceDispatch) {
	VkDescriptorSetAllocateInfo ai = {};
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.pNext = NULL;
	ai.descriptorPool = pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &layout;

	VkDescriptorSet ds;
	if (deviceDispatch->vkAllocateDescriptorSets(device, &ai, &ds) != VK_SUCCESS) {
		DESCRIPTOR_ERROR("Failed to allocate descriptor set");
		// REVIEW: Once again, yucky
		abort();
	}

	return ds;
}