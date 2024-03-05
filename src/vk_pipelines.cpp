#include "vk_pipelines.h"

#include <fstream>
#include <vector>

uint32_t load_shader_module(const char* filepath, VkDevice device, VkShaderModule *outShaderModule,
	DeviceDispatch* deviceDispatch) {
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		fprintf(stderr, "Could not open file %s\n", filepath);
		return 0;
	}

	size_t filesize = (size_t)file.tellg();

	std::vector<uint32_t> buffer(filesize / sizeof(uint32_t));

	file.seekg(0);

	file.read((char*)buffer.data(), filesize);

	file.close();

	VkShaderModuleCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.pNext = NULL;
	ci.codeSize = buffer.size() * sizeof(uint32_t);
	ci.pCode = buffer.data();

	VkShaderModule shaderModule;
	if (deviceDispatch->vkCreateShaderModule(device, &ci, NULL, &shaderModule) != VK_SUCCESS) {
		return 0;
	}
	*outShaderModule = shaderModule;
	return 1;
}