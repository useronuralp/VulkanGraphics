#version 450 core
layout(location = 0) in vec3 v_Color;

// IN
layout(location = 1) in vec2 v_UV;

// OUT
layout(location = 0) out vec4 FragColor;

void main()
{		
  FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}  