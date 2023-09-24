#version 450 core
layout(location = 0) in vec3 v_Color;

// IN
layout(location = 1) in vec2 v_UV;
layout(location = 2) in vec3 v_BoundsMax;
layout(location = 3) in vec3 v_BoundsMin;

// UNFIORMS
layout(set = 0, binding = 1) uniform sampler2D u_WorleyNoiseSampler;

// OUT
layout(location = 0) out vec4 FragColor;

void main()
{		
  
  vec3 color = texture(u_WorleyNoiseSampler, v_UV).rgb;
  FragColor = vec4(color, 1.0);
}  