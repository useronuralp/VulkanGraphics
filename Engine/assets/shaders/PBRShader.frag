#version 450 core

#define MAX_POINT_LIGHT 10

// INs
layout(location = 0) in vec3  v_Pos;
layout(location = 1) in vec2  v_UV;
layout(location = 2) in vec3  v_Normal;
layout(location = 3) in vec4  v_FragPosLightSpace;
layout(location = 4) in mat4  v_ViewMatrix;
layout(location = 8) in mat3 v_TBN;

layout(set = 0, binding = 0) uniform globalUBO
{
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 directionalLightMVP;
    vec4 dirLightPos;
    vec4 cameraPosition;
    vec4 viewportDimension;
    vec4 pointLightPositions[MAX_POINT_LIGHT];
    vec4 pointlightIntensities[MAX_POINT_LIGHT];
    vec4 pointLightColors[MAX_POINT_LIGHT];
    vec4 enablePointLightShadows;
    vec4 directionalLightIntensity;
    mat4 shadowMatrices[MAX_POINT_LIGHT][6];
    vec4 pointFarPlane;
    vec4 pointLightCount;
};

layout(set = 0, binding = 1) uniform sampler2D u_DiffuseSampler;
layout(set = 0, binding = 2) uniform sampler2D u_NormalSampler;
layout(set = 0, binding = 3) uniform sampler2D u_RoughnessMetallicSampler;
layout(set = 0, binding = 4) uniform sampler2D u_DirectionalShadowMap;
layout(set = 0, binding = 5) uniform samplerCube[5] u_PointShadowMap;


layout(location = 0) out vec4 FragColor;
const float PI = 3.14159265359;

vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(u_NormalSampler, v_UV).xyz * 2.0 - 1.0;
    
    vec3 Q1  = dFdx(v_Pos);
    vec3 Q2  = dFdy(v_Pos);
    vec2 st1 = dFdx(v_UV);
    vec2 st2 = dFdy(v_UV);

    vec3 N   = normalize(v_Normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return nom / denom;
}
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


float DirectionalShadowCalculation(vec4 fragPosLightSpace, vec3 fragPos, vec3 lightPosition, vec3 normal)
{
	vec3 shadowCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	float shadowMapDepth  = texture(u_DirectionalShadowMap, shadowCoords.xy).r;
	float currentDepth = shadowCoords.z;


	vec3 lightDir = normalize(v_TBN * lightPosition * mat3(v_ViewMatrix));
	float bias = max(0.001 * (1.0 - dot(normal, lightDir)), 0.001);
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(u_DirectionalShadowMap, 0);
	for (int x = -1; x <= 8; ++x)
	{
		for (int y = -1; y <= 8; ++y)
		{
			float pcfDepth = texture(u_DirectionalShadowMap, shadowCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 100.0;
	if (shadowCoords.z > 1.0)
		shadow = 0.0;


	return shadow;
}

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float PointShadowCalculation(int index, vec3 pointLightPosition)
{
    vec3 fragToLight = v_Pos - pointLightPosition;
    
    float closestDepth = texture(u_PointShadowMap[index], fragToLight).r;
    
    closestDepth *= pointFarPlane.x;
    
    float currentDepth = length(fragToLight);
    
    float bias = 0.05f;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    
    return shadow;

    //float shadow  = 0.0;
    //float bias    = 0.05; 
    //float samples = 20.0;
    //float offset  = 0.1;
    //vec3 fragToLight = v_Pos - pointLightPosition;
    //float currentDepth = length(fragToLight);
    //float viewDistance = length(cameraPosition.xyz - v_Pos);
    //float diskRadius = (1.0 + (viewDistance / 100.0)) / 25.0;
    //for(int i = 0; i < samples; ++i)
    //{
    //    float closestDepth = texture(u_PointShadowMap[index], fragToLight + sampleOffsetDirections[i] * diskRadius).r;
    //    closestDepth *= 100.0;   // undo mapping [0;1]
    //    if(currentDepth - bias > closestDepth)
    //        shadow += 1.0;
    //}
    //shadow /= float(samples);  
    //return shadow;
}


vec3 CalcDirectionalLight(vec3 normal, vec3 viewDir, vec3 dirLightPos, float shadow, vec3 albedo, vec3 roughnessMetallic, vec3 lightColor)
{
   float ao  = roughnessMetallic.r;
   float roughness = roughnessMetallic.g;
   float metallic = roughnessMetallic.b;

   vec3 F0 = vec3(0.04f); 
   F0 = mix(F0, albedo, metallic);
   
   // reflectance equation 
   vec3 Lo = vec3(0.0);
   
   vec3 L = normalize(v_TBN * dirLightPos - v_TBN * v_Pos);
   vec3 H = normalize(viewDir + L);
   
   vec3 radiance = lightColor * directionalLightIntensity.x;        
   
   // cook-torrance brdf
   float NDF  = DistributionGGX(normal, H, roughness);        
   float G    = GeometrySmith(normal, viewDir, L, roughness);      
   vec3  F    = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);       
    
   vec3  numerator    = NDF * G * F;
   float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
   vec3  specular     = numerator / denominator;  
   
   vec3 kS = F;
   vec3 kD = vec3(1.0) - kS;
   kD *= 1.0 - metallic;	
       
   float NdotL = max(dot(normal, L), 0.0);                
   Lo += (kD * vec3(albedo) / PI + specular) * radiance * NdotL; 

   vec3 ambient = vec3(0.01f) * vec3(albedo) * ao;
   vec3 color = ambient + (Lo * (1.0 - shadow));
     
   return color;
}

vec3 CalcPointLight(vec3 normal, vec3 viewDir, vec3 pointLightPos, vec3 albedo, vec3 roughnessMetallic, vec3 lightColor, float intensity, float shadow)
{
   float ao  = roughnessMetallic.r;
   float roughness = roughnessMetallic.g;
   float metallic = roughnessMetallic.b;

   vec3 F0 = vec3(0.04f); 
   F0 = mix(F0, albedo, metallic);
   
   // reflectance equation 
   vec3 Lo = vec3(0.0);
   
   vec3 L = normalize(v_TBN * pointLightPos - v_TBN * v_Pos);
   vec3 H = normalize(viewDir + L);
   
   float linear = 20.0;
   float quadratic = 20.0;
   float constant = 1.0;

   float distance    = length(pointLightPos - v_Pos);
   float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance * distance));  
   vec3 radiance     = lightColor * attenuation * intensity;        
   
   // cook-torrance brdf
   float NDF  = DistributionGGX(normal, H, roughness);        
   float G    = GeometrySmith(normal, viewDir, L, roughness);      
   vec3  F    = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);       
    
   vec3  numerator    = NDF * G * F;
   float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
   vec3  specular     = numerator / denominator;  
   
   vec3 kS = F;
   vec3 kD = vec3(1.0) - kS;
   kD *= 1.0 - metallic;	
       
   float NdotL = max(dot(normal, L), 0.0);                
   Lo += (kD * vec3(albedo) / PI + specular) * radiance * NdotL; 

   vec3 ambient = vec3(0.000f) * vec3(albedo) * ao;
   vec3 color = ambient + (Lo * (1.0 - shadow));
    
   return color;
}

const vec4 v05 = vec4(0.5,0.5,0.5,0.5);



void main()
{		
   // -------Sample the PBR textures here--------- 
   vec3 albedo = pow(texture(u_DiffuseSampler, v_UV).rgb, vec3(2.2));
   vec3 roughnessMetallicTex = texture(u_RoughnessMetallicSampler, v_UV).rgb;
   
   
   // -------Sample the normals from the normal map---------
   vec3 normal = texture(u_NormalSampler, v_UV).rgb;
   normal = normal * 2.0 - 1.0;

   //normal = v_Normal;
   
   vec3 viewDir = normalize(v_TBN * cameraPosition.xyz - v_TBN * v_Pos);
   
   float directionalShadow = DirectionalShadowCalculation(v_FragPosLightSpace, v_Pos, dirLightPos.xyz, normal);
   
   vec3 color = vec3(0.0);
   color += CalcDirectionalLight(normal, viewDir, dirLightPos.xyz, directionalShadow, albedo, roughnessMetallicTex, vec3(1.0, 1.0, 1.0));


   for(int i = 0; i < pointLightCount.x; i++)
   {
        if(enablePointLightShadows.x == 1.0)
        {
            float pointShadow = PointShadowCalculation(i, pointLightPositions[i].xyz);
            color += CalcPointLight(normal, viewDir, pointLightPositions[i].xyz, albedo, roughnessMetallicTex, pointLightColors[i].xyz, pointlightIntensities[i].x, pointShadow);
        }
        else
        {
            color += CalcPointLight(normal, viewDir, pointLightPositions[i].xyz, albedo, roughnessMetallicTex, pointLightColors[i].xyz, pointlightIntensities[i].x, 0.0);
        }
   }

   
   //pointShadow = PointShadowCalculation(1, pointLightPositions[1].xyz);
   //color += CalcPointLight(normal, viewDir, pointLightPositions[1].xyz, albedo, roughnessMetallicTex, vec3(0.97, 0.76, 0.46), pointlightIntensities[1].x, pointShadow);
   //
   //pointShadow = PointShadowCalculation(2, pointLightPositions[2].xyz);
   //color += CalcPointLight(normal, viewDir, pointLightPositions[2].xyz, albedo, roughnessMetallicTex, vec3(0.97, 0.76, 0.46), pointlightIntensities[2].x, pointShadow);
   //
   //pointShadow = PointShadowCalculation(3, pointLightPositions[3].xyz);
   //color += CalcPointLight(normal, viewDir, pointLightPositions[3].xyz, albedo, roughnessMetallicTex, vec3(0.97, 0.76, 0.46), pointlightIntensities[3].x, pointShadow);
   //
   //pointShadow = PointShadowCalculation(4, pointLightPositions[4].xyz);
   //color += CalcPointLight(normal, viewDir, pointLightPositions[4].xyz, albedo, roughnessMetallicTex, vec3(1.0, 0.0, 0.0), pointlightIntensities[4].x, pointShadow);
   

   // Reinhard tonemapping.
   float gamma = 3.2;
   color = color / (color + vec3(1.0));
   color = pow(color, vec3(1.0/gamma)); 

   FragColor = vec4(color, 1.0);
}  