#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 fragPos;

layout (location = 0) out vec4 outFragColor;

// descriptor set 0
layout(set = 0, binding = 0) uniform sceneData {
	vec4 ambience;
} ubo;
layout(set = 0, binding = 1) uniform sampler2D displayTexture;

void main()
{
	vec4 objColor = texture(displayTexture, inUV);
	vec3 ambient = ubo.ambience.rgb * ubo.ambience.a;

	vec3 result = ambient * objColor.rgb;
	outFragColor = vec4(result, 1.0);
}
