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

layout(buffer_reference, std430) uniform buffer SceneBuffer{
	mat4 view;
	mat4 proj;
	mat4 orthoProj;
};

layout(push_constant) uniform constants{
	mat4 model;
	SceneBuffer sceneBuffer;
	VertexBuffer vertexBuffer;
	vec3 viewPos;
} PushConstants;
