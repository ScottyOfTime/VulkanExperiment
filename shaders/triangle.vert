#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) out vec4 outColor;

layout(buffer_reference, std430) readonly buffer SceneBuffer{
	mat4 view;
	mat4 proj;
	mat4 orthoProj;
};

struct Vertex {
	vec4 color;
	vec3 position;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

layout(push_constant) uniform constants{
	SceneBuffer sceneBuffer;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() {
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	SceneBuffer sc = PushConstants.sceneBuffer;

	gl_Position = sc.proj * sc.view * vec4(v.position, 1.0);
	outColor = v.color;
}
