#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outColor;
layout(location = 2) out uint fontIndex;

struct Vertex {
	vec3 position;
	vec2 uv;
	vec3 color;
	uint fontIndex;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer SceneBuffer{
	mat4 view;
	mat4 proj;
	mat4 orthoProj;
};

layout(push_constant) uniform constants{
	SceneBuffer sceneBuffer;
	VertexBuffer vertexBuffer;
	VertexBuffer DONT_USE;
	VertexBuffer DONT_USE_LIGHT_BUFFER;
	uint DONT_USE_EITHER;
	mat4 model;
	vec3 viewPos;
} PushConstants;

void main() {
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	SceneBuffer sc = PushConstants.sceneBuffer;
	mat4 view = mat4(1.0);

	// output data
	vec4 pos = sc.orthoProj * view * 
		vec4(v.position, 1.0f);
	gl_Position = vec4(pos.x, pos.y, 1, pos.w);

	outUV = v.uv;
	outColor = v.color;
	fontIndex = v.fontIndex;
}
