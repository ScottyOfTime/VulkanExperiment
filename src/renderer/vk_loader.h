#ifndef VK_LOADER_H
#define VK_LOADER_H

#include "vk_types.h"
#include <unordered_map>
#include <filesystem>
#include <optional>

// forward declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, const char* filename);

#endif /* VK_LOADER_H */
