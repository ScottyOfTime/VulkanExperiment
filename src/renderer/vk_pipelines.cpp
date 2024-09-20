#include "vk_pipelines.h"
#include "vk_engine.h"

#include <fstream>

void PipelineBuilder::clear() {
	// clear all structs to 0 with correct stype
	inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	colorBlendAttachment = {};
	multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	pipelineLayout = {};
	depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
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

	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
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

void PipelineBuilder::set_vtx_shader(VkShaderModule shader) {
	VkPipelineShaderStageCreateInfo shaderInfo{};
	shaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderInfo.pNext = nullptr;

	shaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderInfo.module = shader;
	shaderInfo.pName = "main";

	shaderStages[0] = shaderInfo;
}

void PipelineBuilder::set_frag_shader(VkShaderModule shader) {
	VkPipelineShaderStageCreateInfo shaderInfo{};
	shaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderInfo.pNext = nullptr;

	shaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderInfo.module = shader;
	shaderInfo.pName = "main";

	shaderStages[1] = shaderInfo;
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
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enable_blending_alphablend() {
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::disable_blending() {
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::set_depth_format(VkFormat format) {
	renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op) {
	depthStencil.depthTestEnable = VK_TRUE;
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

uint32_t load_shader_module(const char* filepath, VkDevice device, 
	VkShaderModule* outShader, EShLanguage stage, DeviceDispatch* deviceDispatch) {
	
	fprintf(stderr, "[Shader Loader] Loading shader %s\n", filepath);
	std::ifstream shaderFile(filepath);
	if (!shaderFile.is_open()) {
		fprintf(stderr, "[Shader Loader] Failed to open shader file %s\n",
			filepath);
		return 1;
	}

	std::string shaderSource((std::istreambuf_iterator<char>(shaderFile)),
		std::istreambuf_iterator<char>());
	shaderFile.close();

	const char* shaderSourceCStr = shaderSource.c_str();

	std::vector<uint32_t> buffer;
	if (compile_shader_to_spirv(buffer, shaderSourceCStr, stage) > 0) {
		return 1;
	}

	VkShaderModuleCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.pNext = NULL;
	ci.codeSize = buffer.size() * sizeof(uint32_t);
	ci.pCode = buffer.data();

	VkShaderModule shaderModule;
	if (deviceDispatch->vkCreateShaderModule(device, &ci, NULL, &shaderModule) != VK_SUCCESS) {
		return 1;
	}
	*outShader = shaderModule;
	return 0;
}

uint32_t compile_shader_to_spirv(std::vector<uint32_t>& spirv, 
	const char* str, EShLanguage stage) {

	glslang::TShader shader(stage);

	shader.setStrings(&str, 1);

	// Set up GLSL compilation options
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan,
		100);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

	TBuiltInResource res = InitResources();
	EShMessages msg = EShMsgDefault;

	if (!shader.parse(&res, 100, false, msg)) {
		fprintf(stderr, "[Shader Loader] Failed to parse shader.\n");
		fprintf(stderr, "\t%s\n", shader.getInfoLog());
		return 1;
	}

	glslang::TProgram program;
	program.addShader(&shader);

	if (!program.link(msg)) {
		fprintf(stderr, "[Shader Loader] Failed to link shader.\n");
		fprintf(stderr, "\t%s\n", shader.getInfoLog());
		return 1;
	}

	glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);

	return 0;
}

static TBuiltInResource InitResources()
{
	TBuiltInResource Resources;

	Resources.maxLights = 32;
	Resources.maxClipPlanes = 6;
	Resources.maxTextureUnits = 32;
	Resources.maxTextureCoords = 32;
	Resources.maxVertexAttribs = 64;
	Resources.maxVertexUniformComponents = 4096;
	Resources.maxVaryingFloats = 64;
	Resources.maxVertexTextureImageUnits = 32;
	Resources.maxCombinedTextureImageUnits = 80;
	Resources.maxTextureImageUnits = 32;
	Resources.maxFragmentUniformComponents = 4096;
	Resources.maxDrawBuffers = 32;
	Resources.maxVertexUniformVectors = 128;
	Resources.maxVaryingVectors = 8;
	Resources.maxFragmentUniformVectors = 16;
	Resources.maxVertexOutputVectors = 16;
	Resources.maxFragmentInputVectors = 15;
	Resources.minProgramTexelOffset = -8;
	Resources.maxProgramTexelOffset = 7;
	Resources.maxClipDistances = 8;
	Resources.maxComputeWorkGroupCountX = 65535;
	Resources.maxComputeWorkGroupCountY = 65535;
	Resources.maxComputeWorkGroupCountZ = 65535;
	Resources.maxComputeWorkGroupSizeX = 1024;
	Resources.maxComputeWorkGroupSizeY = 1024;
	Resources.maxComputeWorkGroupSizeZ = 64;
	Resources.maxComputeUniformComponents = 1024;
	Resources.maxComputeTextureImageUnits = 16;
	Resources.maxComputeImageUniforms = 8;
	Resources.maxComputeAtomicCounters = 8;
	Resources.maxComputeAtomicCounterBuffers = 1;
	Resources.maxVaryingComponents = 60;
	Resources.maxVertexOutputComponents = 64;
	Resources.maxGeometryInputComponents = 64;
	Resources.maxGeometryOutputComponents = 128;
	Resources.maxFragmentInputComponents = 128;
	Resources.maxImageUnits = 8;
	Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
	Resources.maxCombinedShaderOutputResources = 8;
	Resources.maxImageSamples = 0;
	Resources.maxVertexImageUniforms = 0;
	Resources.maxTessControlImageUniforms = 0;
	Resources.maxTessEvaluationImageUniforms = 0;
	Resources.maxGeometryImageUniforms = 0;
	Resources.maxFragmentImageUniforms = 8;
	Resources.maxCombinedImageUniforms = 8;
	Resources.maxGeometryTextureImageUnits = 16;
	Resources.maxGeometryOutputVertices = 256;
	Resources.maxGeometryTotalOutputComponents = 1024;
	Resources.maxGeometryUniformComponents = 1024;
	Resources.maxGeometryVaryingComponents = 64;
	Resources.maxTessControlInputComponents = 128;
	Resources.maxTessControlOutputComponents = 128;
	Resources.maxTessControlTextureImageUnits = 16;
	Resources.maxTessControlUniformComponents = 1024;
	Resources.maxTessControlTotalOutputComponents = 4096;
	Resources.maxTessEvaluationInputComponents = 128;
	Resources.maxTessEvaluationOutputComponents = 128;
	Resources.maxTessEvaluationTextureImageUnits = 16;
	Resources.maxTessEvaluationUniformComponents = 1024;
	Resources.maxTessPatchComponents = 120;
	Resources.maxPatchVertices = 32;
	Resources.maxTessGenLevel = 64;
	Resources.maxViewports = 16;
	Resources.maxVertexAtomicCounters = 0;
	Resources.maxTessControlAtomicCounters = 0;
	Resources.maxTessEvaluationAtomicCounters = 0;
	Resources.maxGeometryAtomicCounters = 0;
	Resources.maxFragmentAtomicCounters = 8;
	Resources.maxCombinedAtomicCounters = 8;
	Resources.maxAtomicCounterBindings = 1;
	Resources.maxVertexAtomicCounterBuffers = 0;
	Resources.maxTessControlAtomicCounterBuffers = 0;
	Resources.maxTessEvaluationAtomicCounterBuffers = 0;
	Resources.maxGeometryAtomicCounterBuffers = 0;
	Resources.maxFragmentAtomicCounterBuffers = 1;
	Resources.maxCombinedAtomicCounterBuffers = 1;
	Resources.maxAtomicCounterBufferSize = 16384;
	Resources.maxTransformFeedbackBuffers = 4;
	Resources.maxTransformFeedbackInterleavedComponents = 64;
	Resources.maxCullDistances = 8;
	Resources.maxCombinedClipAndCullDistances = 8;
	Resources.maxSamples = 4;
	Resources.maxMeshOutputVerticesNV = 256;
	Resources.maxMeshOutputPrimitivesNV = 512;
	Resources.maxMeshWorkGroupSizeX_NV = 32;
	Resources.maxMeshWorkGroupSizeY_NV = 1;
	Resources.maxMeshWorkGroupSizeZ_NV = 1;
	Resources.maxTaskWorkGroupSizeX_NV = 32;
	Resources.maxTaskWorkGroupSizeY_NV = 1;
	Resources.maxTaskWorkGroupSizeZ_NV = 1;
	Resources.maxMeshViewCountNV = 4;

	Resources.limits.nonInductiveForLoops = 1;
	Resources.limits.whileLoops = 1;
	Resources.limits.doWhileLoops = 1;
	Resources.limits.generalUniformIndexing = 1;
	Resources.limits.generalAttributeMatrixVectorIndexing = 1;
	Resources.limits.generalVaryingIndexing = 1;
	Resources.limits.generalSamplerIndexing = 1;
	Resources.limits.generalVariableIndexing = 1;
	Resources.limits.generalConstantMatrixVectorIndexing = 1;

	return Resources;
}