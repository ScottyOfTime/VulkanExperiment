#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 fragPos;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D Textures[];

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

vec3 calc_directional_light(DirectionalLight light, vec3 normal, vec3 viewDir) {
	Material material = PushConstants.materialBuffer.materials[PushConstants.materialID];

	vec3 lightDir = normalize(-light.direction);
	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	//specular shading
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	// combine results
	vec3 ambient = light.ambient * material.ambient;
	vec3 diffuse = light.diffuse * diff * material.diffuse;
	vec3 specular = light.specular * spec * material.specular;
	return (ambient + diffuse + specular);
}

vec3 calc_point_light(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
	Material material = PushConstants.materialBuffer.materials[PushConstants.materialID];

	vec3 lightDir = normalize(light.position - fragPos);
	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	//specular shading
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	// attenuation
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance +
							light.quadratic * (distance * distance));
	vec3 ambient = light.ambient * material.ambient;
	vec3 diffuse = light.diffuse * diff * material.diffuse;
	vec3 specular = light.specular * spec * material.specular;
	ambient *= attenuation;
	diffuse *= attenuation;
	specular *= attenuation;
	return (ambient + diffuse + specular);
}

vec3 calc_spot_light(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
	Material material = PushConstants.materialBuffer.materials[PushConstants.materialID];
	
	vec3 lightDir = normalize(light.position - fragPos);
	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	// specular shading
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);	
	// attenuation
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance +
							light.quadratic * (distance * distance));
	// spotlight intensity
	float theta = dot(lightDir, normalize(-light.direction));
	float epsilon = light.cutOff - light.outerCutOff;
	float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
	// combine results
	vec3 ambient = light.ambient * material.ambient;
	vec3 diffuse = light.diffuse * diff * material.diffuse;
	vec3 specular = light.specular * spec * material.specular;
	ambient *= attenuation * intensity;
	diffuse *= attenuation * intensity;
	specular *= attenuation * intensity;
	return (ambient + diffuse + specular);
}

void main()
{
	vec3 result = vec3(0.0);
	vec3 normal = normalize(inNormal);
	vec3 viewDir = normalize(PushConstants.viewPos - fragPos);

	LightBuffer lightBuffer = PushConstants.lightBuffer;
	uint numDirectionalLights = lightBuffer.numDirectionalLights;
	uint numPointLights = lightBuffer.numPointLights;
	uint numSpotLights = lightBuffer.numSpotLights;
	for (int i = 0; i < numDirectionalLights; i++) {
		DirectionalLight light = lightBuffer.directionalLights.lights[i];
		result += calc_directional_light(light, normal, viewDir);
	}
	for (int i = 0; i < numPointLights; i++) {
		PointLight light = lightBuffer.pointLights.lights[i];
		result += calc_point_light(light, normal, fragPos, viewDir);
	}
	for (int i = 0; i < numSpotLights; i++) {
		SpotLight light = lightBuffer.spotLights.lights[i];
		result += calc_spot_light(light, normal, fragPos, viewDir);
	}
	outFragColor = vec4(result, 1.0);
}
