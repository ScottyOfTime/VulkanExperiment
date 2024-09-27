#ifndef VK_LOADER_H
#define VK_LOADER_H

#include "vk_types.h"
#include <unordered_map>
#include <filesystem>
#include <optional>

// forward declaration
class VulkanEngine;

// Loads the meshes and sends the data to be owned by the vulkan engine
void loadGltfMeshes(VulkanEngine* engine, const char* filename);

#endif /* VK_LOADER_H */
