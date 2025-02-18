#ifndef VK_GLTF_H
#define VK_GLTF_H

#include "vk_types.h"

class VulkanEngine;

void load_gltf_meshes(VulkanEngine* pVulkanEngine, const char* filename);

#endif /* VK_GLTF_H */