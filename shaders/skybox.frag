#version 450

layout(location = 0) in vec3 texCoords;

layout(location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform samplerCube Textures[];

void main() {
	outColor = texture(Textures[1], texCoords);
}
