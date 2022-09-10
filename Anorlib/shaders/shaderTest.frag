#version 450 core

layout(location = 0) in vec3 v_Pos;
layout(location = 1) in vec2 v_UV;
layout(location = 2) in vec3 v_Normal;
layout(location = 3) in vec3 v_DirLightPos;
layout(location = 4) in mat4 v_ViewMat;

layout(set = 0, binding = 5) uniform CameraPosition
{
    vec4 pos;
} camPos;


layout(set = 0, binding = 6) uniform sampler2D u_DiffuseSampler;


layout(location = 0) out vec4 FragColor;

void main()
{		
   vec3 color = pow(texture(u_DiffuseSampler, v_UV).rgb, vec3(2.2));
   vec3 ambient = 0.05 * color;

   vec3 lightDir = normalize(v_DirLightPos - v_Pos);
   vec3 normal = normalize(v_Normal);
   float diff = max(dot(lightDir, normal), 0.0);
   vec3 diffuse = diff * color;

   vec3 viewDir = normalize(camPos.pos.xyz - v_Pos);
   vec3 reflectDir = reflect(-lightDir, normal);
   float spec = 0.0;
   vec3 halfwayDir = normalize(lightDir + viewDir);  
   spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
   vec3 specular = vec3(0.3) * spec; // assuming bright white light color
   
   FragColor = vec4(ambient + diffuse + specular, 1.0);
}  