#ifndef VK_PIPELINES_H
#define VK_PIPELINES_H

#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>

#include <vulkan/vulkan.h>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include "vk_dispatch.h"

// How many pipelines can use a single shader
#define MAX_SHADER_PIPELINES 8

// Functional class that builds a pipleine from options set
// with functions
class PipelineBuilder {
public:
	// index 0 is vertex shader and index 1 is fragment shader
	VkPipelineShaderStageCreateInfo shaderStages[2];

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
	void set_vtx_shader(VkShaderModule shader);
	void set_frag_shader(VkShaderModule shader);
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

struct Shader {
	VkShaderModule shader = VK_NULL_HANDLE;
	const char* path;
	EShLanguage stage;
	std::atomic<uint32_t> recompile;

	std::filesystem::file_time_type lastWrite;
};

struct Pipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;

	uint32_t vtxShaderIdx;
	uint32_t fragShaderIdx;

	PipelineBuilder builder;
};

uint32_t load_shader_module(const char* filepath, VkDevice device,
	VkShaderModule* outShader, EShLanguage stage, DeviceDispatch* deviceDispatch);

uint32_t compile_shader_to_spirv(std::vector<uint32_t>& spirv, 
	const char* str, EShLanguage stage);

// glslang library got rid of their own default constructor for
// resources so have to make my own
static TBuiltInResource InitResources();

#endif /* VK_PIPELINES_H */