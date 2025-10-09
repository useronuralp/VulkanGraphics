#version 450 core

// INs
layout(location = 0) in vec2  v_UV;

layout(set = 0, binding = 0) uniform sampler2D HDRscene;


layout(location = 0) out vec4 FragColor;

float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
void main()
{		
    vec3 color = vec3(0.0);

    color = texture(HDRscene, v_UV).rgb;

   
   // Bright parts extraction.
   float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
   if(brightness > 1.0)
   {
        FragColor = vec4(color.rgb, 1.0);
   }
   else
   {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
   }
}  