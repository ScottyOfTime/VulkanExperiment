#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable


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

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

//>Light structures ---------------------------------------------------------------

struct DirectionalLight {
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

struct PointLight {
	vec3 position;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float constant;
	float linear;
	float quadratic;
};

struct SpotLight {
	vec3 position;
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float constant;
	float linear;
	float quadratic;
	float cutOff;
	float outerCutOff;
};

layout(buffer_reference, std430) readonly buffer DirectionalLightBuffer {
	DirectionalLight lights[];
};

layout(buffer_reference, std430) readonly buffer PointLightBuffer {
	PointLight lights[];
};

layout(buffer_reference, std430) readonly buffer SpotLightBuffer {
	SpotLight lights[];
};

layout(buffer_reference, std430) readonly buffer LightBuffer {
	DirectionalLightBuffer directionalLights;
	PointLightBuffer pointLights;
	SpotLightBuffer spotLights;

	uint numPointLights;
	uint numDirectionalLights;
	uint numSpotLights;
};

//<-----------------------------------------------------------------------------------

layout(push_constant) uniform constants{
	SceneBuffer sceneBuffer;
	VertexBuffer vertexBuffer;
	MaterialBuffer materialBuffer;
	LightBuffer lightBuffer;
	uint materialID;
	mat4 model;
	vec3 viewPos;
} PushConstants;

void main() {
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	SceneBuffer sc = PushConstants.sceneBuffer;

	gl_Position = sc.proj * sc.view * PushConstants.model * vec4(v.position, 1.0); 
}
