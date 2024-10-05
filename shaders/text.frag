#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 inColor;
layout(location = 2) flat in uint fontIndex;

layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D Textures[];

void main() {
	vec4 sampledColor = texture(Textures[fontIndex], uv);
	outColor = vec4(inColor * sampledColor.rgb, sampledColor.a);
}
