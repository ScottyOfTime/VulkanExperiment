#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 fragPos;


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

struct Material {
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float shininess;
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer{
	Material materials[];
};

struct Light {
	vec3 position;
	float constant;
	vec3 direction;
	float linear;
	vec3 color;
	float quadratic;
	float innerAngle;
	float outerAngle;
	float intensity;
	uint type;
	mat4 spaceMatrix;
};

layout(buffer_reference, std430) readonly buffer LightBuffer {
	Light lights[];
};


layout(push_constant) uniform constants{
	SceneBuffer sceneBuffer;
	VertexBuffer vertexBuffer;
	MaterialBuffer materialBuffer;
	LightBuffer lightBuffer;
	uint lightCount;
	uint materialID;
	mat4 model;
	vec3 viewPos;
} PushConstants;

void main() {
	// load vertex data from device address
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	SceneBuffer sc = PushConstants.sceneBuffer;

	// output data
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outNormal = mat3(transpose(inverse(PushConstants.model))) * 
		v.normal;
	fragPos = vec3(PushConstants.model * vec4(v.position, 1.0));
	gl_Position = sc.proj * sc.view * vec4(fragPos, 1.0f);
}
