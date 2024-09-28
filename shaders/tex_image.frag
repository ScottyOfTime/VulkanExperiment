#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 fragPos;
layout (location = 4) in flat uint numLights;

layout (location = 0) out vec4 outFragColor;

struct PointLight {
	vec3 position;
	vec3 color;
	float intensity;	
};

// descriptor set 0
layout(set = 0, binding = 0) uniform sceneData {
	vec4 ambience;
} ubo;
layout(set = 0, binding = 1) uniform sampler2D displayTexture;
layout(set = 0, binding = 2) uniform pointLights {
	PointLight lights[64];
};

void main()
{
	vec4 objColor = texture(displayTexture, inUV);
	vec3 ambient = ubo.ambience.rgb * ubo.ambience.a;

	vec3 norm = normalize(inNormal);
	vec3 diffuse = vec3(0.0);
	for (int i = 0; i < numLights; i++) {
		PointLight p = lights[i];
		vec3 lightDir = normalize(p.position - fragPos);
		float diff = max(dot(norm, lightDir), 0.0);
		vec3 lightDiffuse = diff * p.color * p.intensity;
		diffuse += lightDiffuse;
	}

	objColor = vec4(0.5, 0.5, 1, 1);
	vec3 result = (ambient + diffuse) * objColor.rgb;
	outFragColor = vec4(result, 1.0);
}
