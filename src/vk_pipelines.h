#include <vulkan/vulkan.h>

#include "vk_dispatch.h"

uint32_t load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule,
	DeviceDispatch* deviceDispatch);