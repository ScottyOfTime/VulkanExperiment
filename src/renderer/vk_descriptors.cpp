#include "vk_descriptors.h"

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type,
	uint32_t count, VkDescriptorBindingFlags flags) {
	VkDescriptorSetLayoutBinding newbind = {};
	newbind.binding = binding;
	newbind.descriptorCount = count;
	newbind.descriptorType = type;

	bindings.push_back(newbind);
	bindingFlags.push_back(flags);
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
	bindingFlags.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages,
	DeviceDispatch* deviceDispatch) {
	for (auto& b : bindings) {
		b.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo flags;
	flags.sType = 
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	flags.pNext = nullptr;
	flags.pBindingFlags = bindingFlags.data();
	flags.bindingCount = (uint32_t)bindingFlags.size();

	VkDescriptorSetLayoutCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ci.pNext = &flags;
	ci.pBindings = bindings.data();
	ci.bindingCount = (uint32_t)bindings.size();
	ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

	VkDescriptorSetLayout set;
	if (deviceDispatch->vkCreateDescriptorSetLayout(device, &ci, NULL, &set) != VK_SUCCESS) {
		DESCRIPTOR_ERROR("Failed to create descriptor set layout.");
		// @Review ->	This is yucky
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
	ci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
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

uint32_t DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler,
	VkImageLayout layout, VkDescriptorType type) {
	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.dstArrayElement = imgIndex;
	write.pImageInfo = &info;

	writes.push_back(write);
	return imgIndex++;
}

uint32_t DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, 
	size_t offset, VkDescriptorType type) {
	VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
	});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.dstArrayElement = bufIndex;
	write.pBufferInfo = &info;

	writes.push_back(write);
	return bufIndex++;
}

void DescriptorWriter::clear() {
	imageInfos.clear();
	writes.clear();
	bufferInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set, DeviceDispatch* deviceDispatch) {
	for (size_t i = 0; i < writes.size(); i++) {
		writes[i].dstSet = set;
	}

	deviceDispatch->vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}