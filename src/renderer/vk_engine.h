#ifndef VK_ENGINE_H
#define VK_ENGINE_H

// Right now, all this macro does is prints the deletor queue functions
// as they are called
#define ENGINE_VERBOSE

#include <array>
#include <cmath>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "../os.h"
#include "camera.h"
#include "vk_descriptors.h"
#include "vk_dispatch.h"
#include "vk_loader.h"
#include "vk_pipelines.h"
#include "vk_suballocator.h"
#include "vk_text.h"
#include "vk_types.h"
#include "vk_buffers.h"
#include "vk_context.h"

/*---------------------------
 | MACROS
 ---------------------------*/

#define EngineResult 		uint32_t
#define ENGINE_SUCCESS		0
#define ENGINE_FAILURE		1

#define TIMEOUT_N			1000000000

#define MAX_SHADERS			64
#define MAX_PIPELINES		32
#define MAX_MESHES			128
#define MAX_MATERIALS		64

#define GLOBAL_BUFFER_SIZE	128 * 1024 * 1024
#define UNIFORM_BUFFER_SIZE	16384

#define ENGINE_MESSAGE(MSG) \
	fprintf(stderr, "[VulkanEngine] INFO: " MSG "\n");

#define ENGINE_MESSAGE_ARGS(MSG, ...) \
	fprintf(stderr, "[VulkanEngine] INFO: " MSG "\n", ##__VA_ARGS__);

#define ENGINE_WARNING(MSG) \
	fprintf(stderr, "[VulkanEngine] WARN: " MSG "\n");

#define ENGINE_ERROR(MSG) \
	fprintf(stderr, "[VulkanEngine] ERR: " MSG "\n");

#define ENGINE_RUN_FN(FN) \
	if (FN != ENGINE_SUCCESS) { \
		return ENGINE_FAILURE; \
	}

// This is currently not in use as I like the if + return statements in code ->
// it makes it easier to parse and the engine code and find vulkan function calls
// update: I have started using this instead of the big if statement -> will need
// to refactor existing ones in order to maintain consistency
#define VK_RUN_FN(FN, MSG) \
	if (FN != VK_SUCCESS) { \
		ENGINE_ERROR(MSG); \
		return ENGINE_FAILURE; \
	}

/*---------------------------
 | CONST ARRAYS AND STRUCTS
 ---------------------------*/

const std::array<const char*, 1> vkInstanceExtensions = {
	"VK_KHR_surface"
};

const std::array<const char*, 1> vkDeviceExtensions = {
	"VK_KHR_swapchain"
};

struct DeletionQueue {
	// this struct is for debugging purposes so you can tell
	// what function is being called per iteration in flush()
	// this will be slow but honestly using std::function is already
	// dubious so will be passed in later optimization
	struct NamedFunc {
		const char* name;
		std::function<void()> func;
	};
	std::deque<NamedFunc> deletors;

	void push_function(const char* name, std::function<void()>&& fn) {
		deletors.push_back(NamedFunc{
			.name = name,
			.func = std::move(fn)
		});
	}

	void flush() {
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
#ifdef ENGINE_VERBOSE
			fprintf(stderr, "[DeletorsQueue] Calling function %s\n", it->name);
#endif
			it->func();
		}
		deletors.clear();
	}
};

struct QueueFamilyIndices {
	int32_t graphicsFamily = -1;
	int32_t presentFamily = -1;
};

struct FrameData {
	VkCommandPool cmdPool = NULL;
	VkCommandBuffer cmdBuf;

	VkSemaphore swapchainSemaphore = NULL,
		renderSemaphore = NULL;
	VkFence renderFence = NULL;

	DeletionQueue deletionQueue;
};

struct UploadMeshInfo {
	Vertex*		pVertices;
	uint32_t	vertexCount;

	uint32_t*	pIndices;
	uint32_t	indexCount;

	Surface*	pSurfaces;
	uint32_t	surfaceCount;
};

constexpr unsigned int FRAME_OVERLAP = 2;

/*---------------------------
 | VULKANENGINE CLASS
 ---------------------------*/

class VulkanEngine {
public:
	// TEMPORARY PLEASE MAKE PRIVATE LATER
	FontAtlas				defaultFont;
	EngineResult 			init();
	void 					deinit();

	SDL_Window* 			window;
	VkExtent2D 				windowExtent{ 1280, 720 };
	float 					renderScale = 1.f;

	bool 					resizeRequested = false;
	EngineResult 			resize_swapchain();

	DescriptorAllocator 	descriptorAllocator;
	VkDescriptorSet 		drawImageDescriptors;
	VkDescriptorSetLayout 	drawImageDescriptorLayout;

	VkPipeline 				gradientPipeline;
	VkPipelineLayout 		gradientPipelineLayout;

	// Immediate mode submit structures
	VkFence 				immFence;
	VkCommandBuffer 		immCmdBuf;
	VkCommandPool 			immCmdPool;

	EngineResult 			immediate_submit(std::function<void(VkCommandBuffer)>&& fn);

	/*---------------------------
	 |  INTERFACE
	 ---------------------------*/
	EngineResult			create_shader(const char* path,
								EShLanguage stage, uint32_t* idx);
	EngineResult			create_material(const Material* material,
								uint32_t* idx);

	void		 			upload_mesh(UploadMeshInfo* pInfo);

	// Main commands
	void					begin();
	EngineResult 			draw();

	// Functions to set the state of the renderer
	void					set_active_camera(Camera c);

	void					set_debug_flags(RenderDebugFlags flags);
	void					clear_debug_flags(RenderDebugFlags flags);

	// Functions to record information for drawing
	void					draw_line(glm::vec3 from, glm::vec3 to, glm::vec4 color);
	void					draw_triangle(glm::vec3 vertices[3], glm::vec4 color);

	void					draw_mesh(uint32_t id, const Transform* transform);
	void					draw_wireframe(std::vector<glm::vec3>& vertices,
								std::vector<uint32_t>& indices);
	void					draw_text(const char* text, float x, float y,
								FontAtlas* pAtlas);

	void					add_light(const Light* light);
	

private:
	VmaAllocator 			allocator;
	DeletionQueue 			mainDeletionQueue;

	/*---------------------------
	 |  INITIALIZATION FUNCTIONS
	 ---------------------------*/
#if defined(VK_USE_PLATFORM_XCB_KHR)
	void 					*lib;
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	HMODULE 				lib;
#endif
	EngineResult 			link();
	uint32_t 				shutdown = 0;

	InstanceDispatch 		instanceDispatch;

	VkInstance 				instance = NULL;
	EngineResult 			create_instance();

	VkSurfaceKHR 			surface = NULL;

	VkSurfaceCapabilitiesKHR surfaceCaps;
	std::vector<VkSurfaceFormatKHR> availableFormats;
	std::vector<VkPresentModeKHR> availablePresentModes;

	EngineResult 			create_surface();

	VkPhysicalDevice 		physicalDevice = NULL;
	QueueFamilyIndices 		queueFamilies;
	EngineResult 			find_queue_families(VkPhysicalDevice physdev,
								QueueFamilyIndices *qfi);
	uint32_t 				device_suitable(VkPhysicalDevice dev);
	EngineResult 			select_physical_device();

	VkDevice 				device = NULL;
	VkQueue 				graphicsQueue;
	VkQueue					presentQueue;
	DeviceDispatch 			deviceDispatch;
	EngineResult 			create_device();

	VkSwapchainKHR 			swapchain = NULL;
	VkFormat 				swapchainFormat;
	std::vector<VkImage> 	swapchainImgs;

	std::vector<VkImageView> swapchainImgViews;

	VkExtent2D 				swapchainExtent;
	EngineResult			create_swapchain(uint32_t width, uint32_t height);
	EngineResult 			init_swapchain();
	EngineResult 			destroy_swapchain();

	struct FrameData 		frames[FRAME_OVERLAP];
	uint32_t 				frameNumber = 0;
	struct FrameData&		get_current_frame() 
	{ 
		return frames[frameNumber % FRAME_OVERLAP]; 
	}

	EngineResult 			init_commands();

	EngineResult 			init_sync();

	EngineResult 			init_descriptors();

	EngineResult 			init_pipelines();

	EngineResult 			init_imgui();

	// Draw resources
	AllocatedImage 			drawImage;
	AllocatedImage 			depthImage;
	AllocatedImage			shadowMapAtlas;
	VkExtent2D 				drawExtent;

	/*---------------------------
	 |  RENDER FUNCTIONS
	 ---------------------------*/
	EngineResult		 	render_imgui(VkCommandBuffer cmd, VkImageView targetImgView);

	// The shadow pass for generating shadows on the scene
	EngineResult			render_shadow_pass(VkCommandBuffer cmd);

	// Main pass and its subpasses
	EngineResult			render_main_pass(VkCommandBuffer cmd);
	EngineResult 			render_geometry(VkCommandBuffer cmd);
	EngineResult			render_skybox(VkCommandBuffer cmd);

	// Debug pass and its subpasses
	EngineResult			render_debug_pass(VkCommandBuffer cmd);

	// Primitve drawing
	EngineResult			render_lines(VkCommandBuffer cmd);
	EngineResult			render_triangles(VkCommandBuffer cmd);
	EngineResult			render_wireframes(VkCommandBuffer cmd);
	
	EngineResult			render_text_geometry(VkCommandBuffer cmd);

	/*---------------------------
	 |  FORWARD+ DATA
	 ---------------------------*/
	const uint32_t tileSize = 16;

	/*---------------------------
	 |  RENDERING OBJECTS & VARIABLES
	 ---------------------------*/
	Camera					_activeCamera;
	RenderDebugFlags		_debugFlags = 0;

	/*---------------------------
	 |  THREADS
	 ---------------------------*/
	std::thread				shaderMonitorThread;
	void					shader_monitor_thread();

	/*---------------------------
	 |  DESCRIPTORS
	 ---------------------------*/
	VkDescriptorSetLayout	bindlessDescriptorLayout = VK_NULL_HANDLE;
	VkDescriptorSet			bindlessDescriptorSet = VK_NULL_HANDLE;
	DescriptorAllocator		bindlessDescriptorAllocator{};
	DescriptorWriter		bindlessDescriptorWriter{};

	/*---------------------------
	 |  RESOUCE ARRAYS & BUFFERS
	 ---------------------------*/
	Shader					shaders[MAX_SHADERS];
	uint32_t				shaderCount = 0;
	void					recompile_shader(uint32_t idx);
	void					destroy_shader(uint32_t idx);
	std::mutex				shaderMutex;


	Pipeline				pipelines[MAX_PIPELINES];
	uint32_t				pipelineCount = 0;
	EngineResult			create_pipeline(PipelineBuilder* builder,
								uint32_t vtxShaderIdx, 
								uint32_t fragShaderIdx, 
								uint32_t* idx);
	void					destroy_pipeline(uint32_t idx);

	// Geometry buffer (device local, must copy data into GPU)
	VkBufferSuballocator	geometryBuffer;

	GPUSceneData			sceneData;
	AllocatedBuffer			uSceneData;
	VkDeviceAddress			uSceneDataAddr;

	AllocatedBuffer			lightBuffer;
	VkDeviceAddress			lightBufferAddr;

	// Buffers for text rendering
	AllocatedBuffer			textVertexBuffer;
	VkDeviceAddress			textVertexBufferAddr;
	AllocatedBuffer			textIndexBuffer;

	// Buffer for line rendering
	AllocatedBuffer			lineVertexBuffer;
	VkDeviceAddress			lineVertexBufferAddr;

	// Buffer for debug trianlge rendering (mainly used by JoltPhysics debug renderer)
	AllocatedBuffer			triangleVertexBuffer;
	VkDeviceAddress			triangleVertexBufferAddr;

	// Buffers for wireframe rendering
	AllocatedBuffer			wireframeVertexBuffer;
	VkDeviceAddress			wireframeVertexBufferAddr;
	AllocatedBuffer			wireframeIndexBuffer;
	
	// Renderer owns all meshes loaded/uploaded and are accessed
	// through index
	Mesh					meshes[MAX_MESHES] = {};
	uint32_t				meshCount = 0;

	// 
	AllocatedBuffer			uMaterialBuffer;
	VkDeviceAddress			uMaterialBufferAddr;
	uint32_t				materialCount = 0;

	EngineResult			init_buffers();


	/*---------------------------
	 |  PIPELINES (PIPELINES ARE UINT HANDLES INTO THE PIPELINES ARRAY)
	 ---------------------------*/
	uint32_t				opaquePipeline;
	uint32_t				transparentPipeline;
	EngineResult			init_mesh_pipelines();

	uint32_t				linePipeline;
	EngineResult			init_line_pipeline();

	uint32_t				trianglePipeline;
	EngineResult			init_triangle_pipeline();

	uint32_t				wireframePipeline;
	EngineResult			init_wireframe_pipeline();

	uint32_t				skyboxPipeline;
	EngineResult			init_skybox_pipeline();
	
	uint32_t				textPipeline;
	EngineResult			init_text_pipeline();

	uint32_t				shadowPipeline;
	EngineResult			init_shadow_pipeline();

	/*---------------------------
	 |  DRAW CONTEXTS
	 ---------------------------*/
	DrawContext				_mainDrawContext;

	EngineResult 			init_default_data();

	// default image textures and samplers plus a descriptor set layout
	// for textured images
	AllocatedImage 			errorCheckerboardImage;
	AllocatedImage			spaceCubeMap;
	uint32_t				errorHandle;

	AllocatedImage			containerTexture;

	VkSampler 				defaultSamplerLinear;
	VkSampler 				defaultSamplerNearest;

	AllocatedBuffer			sceneUBO;

	// these two values should be equal by end of program, if not
	// indicates bug
	uint32_t 				buffersCreated = 0;
	uint32_t 				buffersDestroyed = 0;

};

#endif /* VK_ENGINE_H */
