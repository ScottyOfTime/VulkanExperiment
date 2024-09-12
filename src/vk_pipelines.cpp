#include "vk_pipelines.h"
#include "vk_engine.h"

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

void PipelineBuilder::clear() {
	// clear all structs to 0 with correct stype
	inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	colorBlendAttachment = {};
	multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	pipelineLayout = {};
	depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	shaderStages.clear();
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, DeviceDispatch* deviceDispatch) {
	// make viewport state from our stored viewport and scissor
	// at the moment we wont support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = NULL;

	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// setup dummy color blending. we arent using transparent objects yet
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = NULL;

	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	// completely clear VertexInputStateCreateInfo, as we have no need for it
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	// build the actual pipeline
	// we now use all of the info structs we have been writing into this one
	// to create the pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	// connect the renderInfo the the pNext extension mechanism
	pipelineInfo.pNext = &renderInfo;

	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.layout = pipelineLayout;

	VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicInfo.pDynamicStates = &state[0];
	dynamicInfo.dynamicStateCount = 2;

	pipelineInfo.pDynamicState = &dynamicInfo;

	VkPipeline newPipeline;
	if (deviceDispatch->vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &newPipeline)
		!= VK_SUCCESS) {
		ENGINE_WARNING("Failed to create graphics pipeline.");
		return VK_NULL_HANDLE;
	} else {
		return newPipeline;
	}
}

void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader) {
	shaderStages.clear();

	VkPipelineShaderStageCreateInfo shaderInfo{};
	shaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderInfo.pNext = nullptr;

	shaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderInfo.module = vertexShader;
	shaderInfo.pName = "main";

	shaderStages.push_back(shaderInfo);

	shaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderInfo.module = fragmentShader;

	shaderStages.push_back(shaderInfo);
}

void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology) {
	inputAssembly.topology = topology;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::set_polygon_mode(VkPolygonMode mode) {
	rasterizer.polygonMode = mode;
	rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
	rasterizer.cullMode = cullMode;
	rasterizer.frontFace = frontFace;
}

void PipelineBuilder::set_multisampling_none() {
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = NULL;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::set_color_attachment_format(VkFormat format) {
	colorAttachmentFormat = format;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachmentFormats = &colorAttachmentFormat;
}

void PipelineBuilder::enable_blending_additive() {
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enable_blending_alphablend() {
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::disable_blending() {
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::set_depth_format(VkFormat format) {
	renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op) {
	depthStencil.depthBoundsTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = depthWriteEnable;
	depthStencil.depthCompareOp = op;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	depthStencil.minDepthBounds = 0.f;
	depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::disable_depthtest() {
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};
	depthStencil.minDepthBounds = 0.f;
	depthStencil.maxDepthBounds = 1.f;
}