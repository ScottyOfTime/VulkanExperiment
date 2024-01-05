#include <stdio.h>

#include "vk_engine.h"

int main(int argc, char* argv[])
{
	printf("[Main] Running vulkan demo.\n");
	VulkanEngine *vk = new VulkanEngine;
	vk->init();
	vk->deinit();
	delete vk;
	return 0;
}
