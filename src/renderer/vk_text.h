#ifndef VK_TEXT_H
#define VK_TEXT_H

#include "vk_types.h"

#include <stb_truetype.h>

#define FIRST_CHAR	' '
#define LAST_CHAR	'~'
#define MAX_CHAR	97 // ('~' - ' ')

#define ENGINE_ERROR(MSG) \
	fprintf(stderr, "[VulkanEngine Text] ERR: " MSG "\n");

struct FontCreateInfo {
	VmaAllocator	allocator;
	VkDevice		device;
	DeviceDispatch* pDeviceDispatch;

	const char*		ttfPath;
	int				size;

	VkCommandBuffer cmd;
	VkFence			fence;
	VkQueue			queue;
};

struct GlyphInfo {
	glm::vec3 positions[4];
	glm::vec2 uvs[4];
	float offsetX = 0;
	float offsetY = 0;
};

struct FontAtlas {
	AllocatedImage		texture;
	uint32_t			descriptorIndex;
	stbtt_packedchar	charInfo[MAX_CHAR];

	uint32_t			create(FontCreateInfo* createInfo);
	GlyphInfo			get_glyph_info(uint32_t c, float offsetX, float offsetY);
	void				destroy(VkDevice device, DeviceDispatch* deviceDispatch,
							VmaAllocator allocator);
};

#endif /* VK_TEXT_H */