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

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets,
	std::span<PoolSizeRatio> poolRatios, DeviceDispatch* deviceDispatch) {
	ratios.clear();

	for (size_t i = 0; i < poolRatios.size(); i++) {
		ratios.push_back(poolRatios[i]);
	}

	VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios, deviceDispatch);

	setsPerPool = maxSets * 1.5;

	readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device, DeviceDispatch* deviceDispatch) {
	for (size_t i = 0; i < readyPools.size(); i++) {
		deviceDispatch->vkResetDescriptorPool(device, readyPools[i], 0);
	}
	for (size_t i = 0; i < fullPools.size(); i++) {
		deviceDispatch->vkResetDescriptorPool(device, fullPools[i], 0);
		readyPools.push_back(fullPools[i]);
	}
	fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device, DeviceDispatch* deviceDispatch) {
	for (size_t i = 0; i < readyPools.size(); i++) {
		deviceDispatch->vkDestroyDescriptorPool(device, readyPools[i], nullptr);
	}
	readyPools.clear();
	for (size_t i = 0; i < fullPools.size(); i++) {
		deviceDispatch->vkDestroyDescriptorPool(device, fullPools[i], nullptr);
	}
	fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::alloc(VkDevice device, VkDescriptorSetLayout layout,
	DeviceDispatch* deviceDispatch, void* pNext) {
	// get or create a pool to allocate from
	VkDescriptorPool poolToUse = get_pool(device, deviceDispatch);

	VkDescriptorSetAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.pNext = pNext;
	allocInfo.descriptorPool = poolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet ds;
	VkResult res = deviceDispatch->vkAllocateDescriptorSets(device, &allocInfo, &ds);

	if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL) {
		fullPools.push_back(poolToUse);
		poolToUse = get_pool(device, deviceDispatch);
		allocInfo.descriptorPool = poolToUse;

		if (deviceDispatch->vkAllocateDescriptorSets(device, &allocInfo, &ds) != VK_SUCCESS) {
			DESCRIPTOR_ERROR("Could not allocate descriptor sets after reget.");
			return NULL;
		}
	}

	readyPools.push_back(poolToUse);
	return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device, DeviceDispatch* deviceDispatch) {
	VkDescriptorPool newPool;
	if (readyPools.size() != 0) {
		newPool = readyPools.back();
		readyPools.pop_back();
	} else {
		newPool = create_pool(device, setsPerPool, ratios, deviceDispatch);

		setsPerPool = setsPerPool * 1.5;
		if (setsPerPool > 4092) {
			setsPerPool = 4092;
		}
	}
	return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount,
	std::span<PoolSizeRatio> poolRatios, DeviceDispatch* deviceDispatch) {
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount)
		});
	}

	VkDescriptorPoolCreateInfo poolInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags = 0;
	poolInfo.maxSets = setCount;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool;
	deviceDispatch->vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);
	return newPool;
}

void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler,
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
	write.pImageInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
	VkDescriptorType type) {
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
	write.pBufferInfo = &info;

	writes.push_back(write);
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