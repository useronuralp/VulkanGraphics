#version 450 core

// INs
layout (location = 0) in vec2 v_UV;

layout(location = 0) out vec4 FragColor;

layout (binding = 0) uniform sampler2D first;
layout (binding = 1) uniform sampler2D second;

vec4 UpsampleTent()
{

    vec2 texelSize = 1.0 / vec2(textureSize(first, 0));

    vec4 d = texelSize.xyxy * vec4(1.0, 1.0, -1.0, 0.0);

    vec4 s;
    s =  texture(first, v_UV - d.xy);
    s += texture(first, v_UV - d.wy) * 2.0;
    s += texture(first, v_UV - d.zy);
         
    s += texture(first, v_UV + d.zw) * 2.0;
    s += texture(first, v_UV       ) * 4.0;
    s += texture(first, v_UV + d.xw) * 2.0;
         
    s += texture(first, v_UV + d.zy);
    s += texture(first, v_UV + d.wy) * 2.0;
    s += texture(first, v_UV + d.xy);

    return s * (1.0 / 16.0);
}


void main()
{		

    vec4 upscaled = UpsampleTent();

	vec4 scnd = texture(second, v_UV);
	vec4 final;
	final.x = upscaled.x + scnd.x;
	final.y = upscaled.y + scnd.y;
	final.z = upscaled.z + scnd.z;
	final.w = 1.0;
   FragColor = vec4(final.rgb, 1.0); 
}  