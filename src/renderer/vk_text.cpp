#include "vk_text.h"

#include "vk_images.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

uint32_t FontAtlas::create(FontCreateInfo* createInfo) {
	FILE* fontFile = fopen(createInfo->ttfPath, "rb");
	if (!fontFile) {
		ENGINE_ERROR("Failed to load default font.");
		return 1;
	}

	fseek(fontFile, 0, SEEK_END);
	long fileSize = ftell(fontFile);
	fseek(fontFile, 0, SEEK_SET);

	unsigned char* fontBuffer = (unsigned char*)malloc(fileSize);
	if (!fontBuffer) {
		ENGINE_ERROR("Failed to allocate font buffer.");
		free(fontBuffer);
		return 1;
	}

	fread(fontBuffer, fileSize, 1, fontFile);
	fclose(fontFile);

	const int atlasWidth = 512;
	const int atlasHeight = 512;

	unsigned char* atlas = (unsigned char*)malloc(atlasWidth * atlasHeight);
	if (atlas == nullptr) {
		return 1;
	}
	stbtt_pack_context pc;
	if (!stbtt_PackBegin(&pc, atlas, atlasWidth, atlasHeight, 0, 1, nullptr)) {
		ENGINE_ERROR("Failed to initialize font packing.");
		return 1;
	}

	stbtt_PackSetOversampling(&pc, 2, 2);
	if (!stbtt_PackFontRange(&pc, fontBuffer, 0, createInfo->size, ' ', MAX_CHAR,
		charInfo)) {
		ENGINE_ERROR("Failed to pack font.");
		return 1;
	}

	stbtt_PackEnd(&pc);

	unsigned char* rgbaAtlas = (unsigned char*)malloc(atlasWidth * atlasHeight * 4);
	if (rgbaAtlas == nullptr) {
		return 1;
	}
	for (size_t i = 0; i < atlasWidth * atlasHeight; i++) {
		float alpha = atlas[i] / 255.f;

		rgbaAtlas[i * 4] = (unsigned char)(atlas[i] * alpha);
		rgbaAtlas[i * 4 + 1] = (unsigned char)(atlas[i] * alpha);
		rgbaAtlas[i * 4 + 2] = (unsigned char)(atlas[i] * alpha);
		rgbaAtlas[i * 4 + 3] = atlas[i];
	}

	ImageCreateInfo imgInfo = {};
	imgInfo.allocator = createInfo->allocator;
	imgInfo.device = createInfo->device;
	imgInfo.pDeviceDispatch = createInfo->pDeviceDispatch;
	imgInfo.pImg = &texture;
	imgInfo.size = VkExtent3D{ atlasWidth, atlasHeight, 1 };
	imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	create_image_with_data(&imgInfo, rgbaAtlas, createInfo->cmd, createInfo->fence,
		createInfo->queue);

	free(atlas);
	free(rgbaAtlas);
	free(fontBuffer);
	return 0;
}

GlyphInfo FontAtlas::get_glyph_info(uint32_t c, float offsetX, float offsetY) {
	stbtt_aligned_quad q;
	stbtt_GetPackedQuad(charInfo, texture.imageExtent.width,
		texture.imageExtent.height, c - FIRST_CHAR, &offsetX, &offsetY,
		&q, 1);

	GlyphInfo info = {};
	info.offsetX = offsetX;
	info.offsetY = offsetY;
	info.positions[0] = { q.x0, q.y1, 0 };
	info.positions[1] = { q.x0, q.y0, 0 };
	info.positions[2] = { q.x1, q.y0, 0 };
	info.positions[3] = { q.x1, q.y1, 0 };
	info.uvs[0] = { q.s0, q.t1 };
	info.uvs[1] = { q.s0, q.t0 };
	info.uvs[2] = { q.s1, q.t0 };
	info.uvs[3] = { q.s1, q.t1 };

	return info;
}

void FontAtlas::destroy(VkDevice device, DeviceDispatch* deviceDispatch,
	VmaAllocator allocator) {
	destroy_image(device, deviceDispatch, allocator, &texture);
}