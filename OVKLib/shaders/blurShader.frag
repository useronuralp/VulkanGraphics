#version 450 core

// INs
layout (location = 0) in vec2 v_UV;

layout(set = 0, binding = 0) uniform sampler2D u_Texture;

layout(location = 0) out vec4 FragColor;

vec4 DownsampleBox13Tap()
{
	vec2 texelSize = 1.0 / vec2(textureSize(u_Texture, 0));

	vec4 A = texture(u_Texture, v_UV + texelSize * vec2(-1.0, -1.0));
	vec4 B = texture(u_Texture, v_UV + texelSize * vec2(0.0, -1.0));
	vec4 C = texture(u_Texture, v_UV + texelSize * vec2(1.0, -1.0));
	vec4 D = texture(u_Texture, v_UV + texelSize * vec2(-0.5, -0.5));
	vec4 E = texture(u_Texture, v_UV + texelSize * vec2(0.5, -0.5));
	vec4 F = texture(u_Texture, v_UV + texelSize * vec2(-1.0,  0.0));
	vec4 G = texture(u_Texture, v_UV);
	vec4 H = texture(u_Texture, v_UV + texelSize * vec2(1.0,  0.0));
	vec4 I = texture(u_Texture, v_UV + texelSize * vec2(-0.5,  0.5));
	vec4 J = texture(u_Texture, v_UV + texelSize * vec2(0.5,  0.5));
	vec4 K = texture(u_Texture, v_UV + texelSize * vec2(-1.0,  1.0));
	vec4 L = texture(u_Texture, v_UV + texelSize * vec2(0.0,  1.0));
	vec4 M = texture(u_Texture, v_UV + texelSize * vec2(1.0,  1.0));

    vec2 div = (1.0 / 4.0) * vec2(0.5, 0.125);

    vec4 o = (D + E + I + J) * div.x;
    o += (A + B + G + F) * div.y;
    o += (B + C + H + G) * div.y;
    o += (F + G + L + K) * div.y;
    o += (G + H + M + L) * div.y;

    return o;
}

vec4 DownsampleBox4Tap()
{
	vec2 texelSize = 1.0 / vec2(textureSize(u_Texture, 0));
    vec4 d = texelSize.xyxy * vec4(-1.0, -1.0, 1.0, 1.0);

    vec4 s;
    s =  texture(u_Texture, v_UV + d.xy);
    s += texture(u_Texture, v_UV + d.zy);
    s += texture(u_Texture, v_UV+ d.xw);
    s += texture(u_Texture, v_UV + d.zw);

    return s * (1.0 / 4.0);
}

void main()
{		
    FragColor = vec4(DownsampleBox13Tap().rgb, 1.0);
}  