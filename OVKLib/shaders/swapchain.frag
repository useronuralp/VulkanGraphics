#version 450 core

// INs
layout (location = 0) in vec2 v_UV;

layout(location = 0) out vec4 FragColor;

layout (binding = 0) uniform sampler2D scene;

vec3 aces(vec3 color) {
	const mat3 inputMatrix = mat3(vec3(0.59719, 0.07600, 0.02840), vec3(0.35458, 0.90834, 0.13383), vec3(0.04823, 0.01566, 0.83777));
	const mat3 outputMatrix = mat3(vec3(1.60475, -0.10208, -0.00327), vec3(-0.53108, 1.10813, -0.07276), vec3(-0.07367, -0.00605, 1.07602));

	vec3 newColor = inputMatrix * color;

	vec3 a = newColor * (newColor + 0.0245786) - 0.000090537;
	vec3 b = newColor * (0.983729 * newColor + 0.4329510) + 0.238081;
	newColor = a / b;
	
	return outputMatrix * newColor;
}


vec3 reinhard(vec3 color)
{
	return color / (color + vec3(1.0));
}

void main()
{		


	vec2 uv;
	uv.x = v_UV.x;
	uv.y = 1 - v_UV.y;

	// NICE LITTLE BLACK OUTLINE EFFECT I RANDOMLY FOUND ON THE INTERNET LOL
	//vec4 outline = texture(scene, uv);
	//vec4 offset = texture(scene, uv - 0.003);
	//if(length(outline - offset) > 0.1)
	//{
	//	outline = vec4(0);
	//}


   FragColor = vec4(texture(scene, uv).rgb, 1.0);
}  