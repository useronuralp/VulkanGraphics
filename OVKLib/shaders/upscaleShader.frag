#version 450 core

// INs
layout (location = 0) in vec2 v_UV;

layout(location = 0) out vec4 FragColor;

layout (binding = 0) uniform sampler2D first;
layout (binding = 1) uniform sampler2D second;

void main()
{		
	vec4 scnd = texture(second, v_UV);
	vec4 frst = texture(first, v_UV);
	vec4 final;
	final.x = frst.x + scnd.x;
	final.y = frst.y + scnd.y;
	final.z = frst.z + scnd.z;
	final.w = frst.w + scnd.w;
   FragColor = vec4(final.rgb, 1.0);
}  