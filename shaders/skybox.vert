#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 texCoords;

struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
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
	vec3 viewProj;
} PushConstants;

void main() {
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	SceneBuffer sc = PushConstants.sceneBuffer;

	mat4 viewNoTranslate = mat4(mat3(sc.view));
	vec4 pos = sc.proj * viewNoTranslate *
		vec4(v.position, 1.0);

	gl_Position = pos.xyww;
	texCoords = v.position;
}
