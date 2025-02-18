#version 450
#extension GL_EXT_buffer_reference : require

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

layout(push_constant) uniform constants {
	mat4 model;
	mat4 lightSpaceMatrix;	
	VertexBuffer vertexBuffer;
} pc;

void main() {
	Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
	gl_Position = pc.lightSpaceMatrix * pc.model 
		* vec4(v.position, 1.0);
}
