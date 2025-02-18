#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 fragPos;

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

float calc_shadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords.xy = projCoords.xy * 0.5 + 0.5;
	float closestDepth = texture(Textures[4], projCoords.xy).r;
	float currentDepth = projCoords.z;
	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
	return shadow;
}

vec3 calc_directional_light(Light light, vec3 normal, vec3 viewDir) {
	Material material = PushConstants.materialBuffer.materials[PushConstants.materialID];
	vec3 lightColor = light.color * light.intensity;

	vec3 lightDir = normalize(-light.direction);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	//specular shading
	float spec = pow(max(dot(viewDir, halfwayDir), 0.0), material.shininess);
	// combine results
	vec3 ambient = lightColor;
	vec3 diffuse = diff * lightColor;
	vec3 specular = spec * lightColor; 
	vec4 fragPosLightSpace = light.spaceMatrix * vec4(fragPos, 1.0);
	float shadow = calc_shadow(fragPosLightSpace, normal, lightDir);
	return (ambient + (1.0 - shadow) * (diffuse + specular)) * material.diffuse;
}

vec3 calc_point_light(Light light, vec3 normal, vec3 fragPos, vec3 viewDir) {
	Material material = PushConstants.materialBuffer.materials[PushConstants.materialID];

	vec3 lightDir = normalize(light.position - fragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	//specular shading
	float spec = pow(max(dot(viewDir, halfwayDir), 0.0), 32);
	// attenuation
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance +
							light.quadratic * (distance * distance));
	vec3 ambient = light.color * material.ambient;
	vec3 diffuse = light.color * diff * material.diffuse;
	vec3 specular = light.color * spec * material.specular;
	ambient *= attenuation;
	diffuse *= attenuation;
	specular *= attenuation;
	return (ambient + diffuse + specular);
}

vec3 calc_spot_light(Light light, vec3 normal, vec3 fragPos, vec3 viewDir) {
	Material material = PushConstants.materialBuffer.materials[PushConstants.materialID];
	
	vec3 lightDir = normalize(light.position - fragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	// specular shading
	float spec = pow(max(dot(viewDir, halfwayDir), 0.0), material.shininess);	
	// attenuation
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance +
							light.quadratic * (distance * distance));
	// spotlight intensity
	float theta = dot(lightDir, normalize(-light.direction));
	float epsilon = light.innerAngle - light.outerAngle;
	float intensity = clamp((theta - light.outerAngle) / epsilon, 0.0, 1.0);
	// combine results
	vec3 ambient = light.color * material.ambient;
	vec3 diffuse = light.color * diff * material.diffuse;
	vec3 specular = light.color * spec * material.specular;
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
	for (int i = 0; i < PushConstants.lightCount; i++) {
		Light light = lightBuffer.lights[i];

		if (light.type == 0) { // Directional light
			result += calc_directional_light(light, normal, viewDir);
		} else if (light.type == 1) { // Point light
			result += calc_point_light(light, normal, fragPos, viewDir);
		} else if (light.type == 2) { // Spot light 
			result += calc_spot_light(light, normal, fragPos, viewDir);
		}
		
	}
	outFragColor = vec4(result, 1.0);
}
