#ifndef VK_PIPELINES_H
#define VK_PIPELINES_H

#include <vulkan/vulkan.h>
#include <vector>

#include "vk_dispatch.h"

uint32_t load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule,
	DeviceDispatch* deviceDispatch);

class PipelineBuilder {
public:
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineLayout pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineRenderingCreateInfo renderInfo;
	VkFormat colorAttachmentFormat;

	PipelineBuilder() { clear(); }

	void clear();

	VkPipeline build_pipeline(VkDevice device, DeviceDispatch* deviceDispatch);
	void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
	void set_input_topology(VkPrimitiveTopology topology);
	void set_polygon_mode(VkPolygonMode mode);
	void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
	void set_multisampling_none();
	void set_color_attachment_format(VkFormat format);

	void enable_blending_additive();
	void enable_blending_alphablend();
	void disable_blending();
	
	void set_depth_format(VkFormat format);
	void enable_depthtest(bool depthWriteEnable, VkCompareOp op);
	void disable_depthtest();
};

#endif /* VK_PIPELINES_H */