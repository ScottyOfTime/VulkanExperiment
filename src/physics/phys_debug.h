#ifndef PHYS_DEBUG_H
#define PHYS_DEBUG_H

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

#include "../renderer/vk_engine.h"



using namespace JPH;

class DebugRendererSimpleImpl final : public DebugRendererSimple {
public:
	DebugRendererSimpleImpl(VulkanEngine* pVulkanEngine) : _pVulkanEngine(pVulkanEngine) {};

	virtual void DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) override;
	virtual void DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3,
		ColorArg inColor, ECastShadow inCastShadow) override;
	virtual void DrawText3D(RVec3Arg inPosition,
		const string_view& inString, ColorArg inColor,
		float inHeight) override;
private:
	VulkanEngine* _pVulkanEngine;
};

#endif /* PHYS_DEBUG_H */