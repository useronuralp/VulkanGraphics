#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_FragPosLightSpace;

layout(binding = 4) uniform sampler2D depthMap;


float LinearizeDepth(float depth)
{
  float n = 200.0;
  float f = 700.0;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main()
{
	vec4 shadowCoords = v_FragPosLightSpace / v_FragPosLightSpace.w;
	float depthValue = texture(depthMap, v_UV).r;
	//FragColor = vec4(vec3(depthValue), 1.0);
	if(depthValue < 1.0)
	{
		depthValue = (depthValue / 1.0) * 0.2;
	}
	FragColor =  vec4(vec3(depthValue), 1.0);
	
	
	//float depth = texture(depthMap, v_UV).r;
	//FragColor = vec4(vec3(1.0 - LinearizeDepth(depth)), 1.0);
}