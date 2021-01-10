#version 440 core

#define MAX_POINT_LIGHTS 3
#define MAX_POINT_SHADOWS 3
#define MIN_VARIANCE 0.00001
#define LIGHT_BLEED_REDUCTION_AMOUNT 1.0
#define NUM_LIGHT_PROBES 30

layout (location = 0) out vec3 FragColor;
layout (location = 1) out vec3 BrightColor;

struct DirectionalLight {
	vec3 position;
	vec3 direction;
	vec3 color;
};

struct PointLight {
	vec3 position;
	
	vec3 color;
		
	float kc;
	float kl;
	float kq;
};

struct LightProbe
{
	vec3 position;
	int irradiance_cubemap;
};
// ------------------------

uniform LightProbe light_probes[NUM_LIGHT_PROBES];
uniform samplerCubeArray diffuse_irradiance_light_probe_cubemaps;
uniform samplerCubeArray specular_irradiance_light_probe_cubemaps;
uniform sampler2D brdfLUT;

// ------------------------

vec3 FragPos;
vec4 FragPosLightSpace;
vec3 tangent;

in vec2 TexCoords;
in vec3 view_ray;


// -----------------------

uniform DirectionalLight directionalLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

uniform mat4 directional_light_space_matrix;
uniform bool directional_lighting_enabled;

uniform int N_POINT;

// -----------------------

uniform float near;
uniform float far;
uniform int use_ssao;

// -----------------------

// Shadow Textures -------------------------------
uniform sampler2D directional_shadow_depth_map;
uniform samplerCube point_shadow_depth_map[MAX_POINT_SHADOWS];

// -----------------------

uniform vec3 camera_pos;
uniform vec3 camera_look_direction;
uniform mat4 inv_projection;
uniform mat4 inv_view;

uniform sampler2D diffuse_texture; //albedo
uniform sampler2D normal_texture;
uniform sampler2D depth_texture;
uniform sampler2D ssao_texture;
uniform sampler2D mor_texture; // Metallic, Occlusion, Roughness
uniform float positive_exponent;
uniform float negative_exponent;
// -----------------------

const float PI = 3.14159265359;

// -----------------------

vec3 fresnelSchlick(float cosine, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosine, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float R) // Normal, Halfway, Roughness
{
	float a = R * R;
	float a2 = a * a;

	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return num/denom;
}

float GeometrySchlickGGX(float NdotV, float R)
{
	float r = (R + 1.0);
	float k = r * r / 8.0;

	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float R)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, R);
	float ggx1 = GeometrySchlickGGX(NdotL, R);

	return ggx1 * ggx2;
}

float fabs(float v)
{
	return v < 0 ? -v : v;
}

vec3 BloomThresholdFilter(vec3 in_color, float threshold)
{
	float intensity = dot(in_color, vec3(0.2126, 0.7152, 0.0722));
	if(intensity > threshold)
	{
		return in_color;
	}
	else
	{
		return vec3(0.0);
	}
}

float linstep(float mi, float ma, float v)
{
	return clamp ((v - mi)/(ma - mi), 0, 1);
}

float ReduceLightBleeding(float p_max, float Amount) 
{
	return linstep(Amount, 1, p_max); 
} 

vec2 warpDepth(float depth)
{
    depth = 2.0f * depth - 1.0f;
    float pos = exp(positive_exponent * depth);
    float neg = -exp(-negative_exponent * depth);
    vec2 wDepth = vec2(pos, neg);
    return wDepth;
}

float chebyshevUpperBound(vec2 moments, float mean, float minVariance, int light_type) // light_type : 0 - directional, 1 - point
{
	float shadow = 1.0;
    if(mean <= moments.x)
    {
        return 1.0;
    }
    else
    {
        float variance = moments.y - (moments.x * moments.x);
        variance = max(variance, minVariance);
        float d = mean - moments.x;
        shadow = variance / (variance + (d * d));
		if(light_type == 1)
		{
			return ReduceLightBleeding(shadow, LIGHT_BLEED_REDUCTION_AMOUNT);
		}
        else
			return shadow;
    }
}



float CheckDirectionalShadow(float bias, vec3 lightpos, vec3 FragPos)
{		
	vec3 projCoords = FragPosLightSpace.xyz;// / FragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	vec4 closest_depth = texture(directional_shadow_depth_map, projCoords.xy, 0).rgba;

	//vec4 closest_depth_r = textureGather(directional_shadow_depth_map, projCoords.xy, 0);
	//vec4 closest_depth_g = textureGather(directional_shadow_depth_map, projCoords.xy, 1);

	//vec2 avg_m = vec2((closest_depth_r.x + closest_depth_r.y + closest_depth_r.z + closest_depth_r.w) / 4.0, 
					  //(closest_depth_g.x + closest_depth_g.y + closest_depth_g.z + closest_depth_g.w) / 4.0);

	//float cup =  chebyshevUpperBound(length(FragPos - lightpos) / far, closest_depth);

	vec2 pos_moments = vec2(closest_depth.x, closest_depth.z);
	vec2 neg_moments = vec2(closest_depth.y, closest_depth.w);

	vec2 wDepth = warpDepth(length(FragPos - lightpos) / far);

	vec2 depthScale = 0.01 * vec2(positive_exponent, negative_exponent) * wDepth;
	vec2 minVariance = depthScale * depthScale;
	float pos_result = chebyshevUpperBound(pos_moments, wDepth.x, minVariance.x, 0);
	float neg_result = chebyshevUpperBound(neg_moments, wDepth.y, minVariance.y, 0);

	return min(pos_result, neg_result);
}

vec3 CalcDirectional(vec3 FPos, vec3 V, vec3 N, vec3 mor, vec3 F0, vec3 albedo)
{
	vec3 Lo = vec3(0.0);

	vec3 L = normalize(directionalLight.position);
	vec3 halfway = normalize(V + L);

	vec3 radiance = directionalLight.color;

	float NDF = DistributionGGX(N, halfway, mor.b);
	float G = GeometrySmith(N, V, L, mor.b);
	vec3 F = fresnelSchlick(max(dot(halfway, V), 0.0), F0, mor.z);

	vec3 numerator = NDF * G * F;
	float denominator = 4 * max(dot(N, V), 0.001) * max(dot(N, L), 0.001) + 0.0001;
	vec3 specular = numerator / denominator;

	vec3 kd = vec3(1.0) - F; // kD = 1.0 - kS
		
	kd *= 1.0 - mor.r;

	float NdotL = max(dot(N, L), 0.0);
	Lo = (kd * albedo / PI + specular) * radiance * NdotL;

	return Lo;
}

float samplePointShadow(float bias, float current_depth, vec3 light_to_frag_direction,int shadow_map_index)
{
	float s = 0.0;

	vec2 static_depth = texture(point_shadow_depth_map[shadow_map_index], light_to_frag_direction).rg;
	vec2 dynamic_depth = texture(point_shadow_depth_map[shadow_map_index], light_to_frag_direction).ba;
	
	vec2 closest_depth = static_depth.x < dynamic_depth.x ? static_depth : dynamic_depth; 

	s = chebyshevUpperBound(closest_depth, current_depth, MIN_VARIANCE, 1);

	return s;
}

float CheckPointShadow(vec3 point_light_pos,int index, float bias, vec3 FragPos)
{
	float shadow = 0.0;

	vec3 light_to_frag = FragPos - point_light_pos;
	float current_depth = length(light_to_frag) / far;

	shadow = samplePointShadow(bias,current_depth, normalize(light_to_frag), index);

	return shadow;
}

vec3 CalcPoint(int index, vec3 FPos, vec3 V, vec3 N, vec3 mor, vec3 F0, vec3 albedo) 
{
	vec3 Lo = vec3(0.0);
	
	vec3 L = (pointLights[index].position - FPos);
	vec3 halfway = normalize(V + normalize(L));

	float dist = length(L);
	float attenuation = 1.0 / (dist * dist);
	vec3 radiance = pointLights[index].color * attenuation;

	float NDF = DistributionGGX(N, halfway, mor.b);
	float G = GeometrySmith(N, V, normalize(L), mor.b);
	vec3 F = fresnelSchlick(max(dot(halfway, V), 0.0), F0, mor.z);

	vec3 numerator = NDF * G * F;
	float denominator = 4 * max(dot(N, V), 0.001) * max(dot(N, normalize(L)), 0.001) + 0.0001;
	vec3 specular = numerator / denominator;

	vec3 kd = vec3(1.0) - F; // kD = 1.0 - kS
		
	kd *= 1.0 - mor.r;

	float NdotL = max(dot(N, normalize(L)), 0.0);
	Lo = (kd * albedo / PI + specular) * radiance * NdotL;
	
	return Lo;
}

vec3 ScreenToWorldPos()
{
	float z = texture(depth_texture, TexCoords).r * 2.0 - 1.0;

	vec4 clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4 viewSpacePosition = inv_projection * clipSpacePosition;

	viewSpacePosition /= viewSpacePosition.w;

	return (inv_view * viewSpacePosition).xyz;
}

int FindClosestProbe(vec3 position)
{
	int cpi = 0;
	float min_d = 1000000.0;
	for(int i=0; i<NUM_LIGHT_PROBES; i++)
	{
		float d = length(light_probes[i].position - FragPos);
		if(d < min_d)
		{
			min_d = d;
			cpi = i;
		}
	}

	return cpi;
}

vec3 ParallaxCorrect(vec3 position, vec3 viewdir, vec3 direction, vec3 light_probe_position)
{
	vec3 BoxMax = light_probe_position + vec3(1.0) * 2.0;
	vec3 BoxMin = light_probe_position - vec3(-1.0) * 2.0;

	vec3 FirstPlaneIntersect = (BoxMax - position) / direction;
	vec3 SecondPlaneIntersect = (BoxMin - position) / direction;

	vec3 FurthestPlane = max(FirstPlaneIntersect, SecondPlaneIntersect);

	float dist = min(min(FurthestPlane.x, FurthestPlane.y), FurthestPlane.z);

	vec3 X = position + direction * dist;

	return normalize(X - light_probe_position);
}

void main()
{
	vec3 color = vec3(0.0);
	vec3 albedo = pow(texture(diffuse_texture, TexCoords).rgb, vec3(2.0));
	vec3 mor = texture(mor_texture, TexCoords).rgb;
	vec3 normal = normalize(texture(normal_texture, TexCoords).xyz * 2.0 - 1.0);
	float ssao = 1.0;
	vec3 irradiance;
	vec3 ambient;
	vec3 specular;
	vec3 specularIrradiance;
	vec2 envBRDF;

	if(use_ssao == 1)
	{
		ssao = texture(ssao_texture, TexCoords).r;
	}

	FragPos = ScreenToWorldPos();
	FragPosLightSpace = directional_light_space_matrix * vec4(FragPos,1.0);

	vec3 viewdir = normalize(camera_pos - FragPos);

	
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, mor.r);
	vec3 Lo = vec3(0.0);

	vec3 kS = fresnelSchlick(max(dot(normal, viewdir), 0.0), F0, mor.z);
	vec3 kD = 1.0 - kS;
	
	int closest_probe_index = FindClosestProbe(FragPos);
	
	irradiance = texture(diffuse_irradiance_light_probe_cubemaps, vec4(normal, closest_probe_index)).rgb;

	vec3 diffuse = irradiance * albedo;

	vec3 reflection = ParallaxCorrect(FragPos, -viewdir, normalize(reflect(-viewdir, normal)), light_probes[closest_probe_index].position);

	const float MAX_REFLECTION_LOD = 6.0;
	specularIrradiance = textureLod(specular_irradiance_light_probe_cubemaps, vec4(reflection, closest_probe_index), mor.z * MAX_REFLECTION_LOD).rgb;
	vec2 brdfTexCoord = vec2(max(dot(normal, viewdir), 0.0), mor.z);

	envBRDF = texture(brdfLUT, vec2(brdfTexCoord.x, brdfTexCoord.y)).rg;
	specular = specularIrradiance * (kS  * envBRDF.x + envBRDF.y);
	ambient = (kD * diffuse + specular) * (1.0 - mor.g);
	

	if(directional_lighting_enabled)
	{
		float bias = 0.000001;
		//bias = max(0.01 * (1.0 - dot(normalize(normal), normalize(-directionalLight.direction))), 0.001);
		float directional_shadow = CheckDirectionalShadow(bias, directionalLight.position, FragPos);

		Lo = CalcDirectional(FragPos, viewdir, normal, mor, F0, albedo);
		
		color += max((ambient * 1.0) + (Lo * directional_shadow), vec3(0.0)) * ssao;
	}

	for(int i=0; i<N_POINT; i++)
	{
		float point_shadow = 1.0;
		float bias = 0.01;
		//bias = max(0.01 * (1 - dot(normal, normalize(lightdir))), 0.001);

		if(i < MAX_POINT_SHADOWS) // This could be slow
		{
			point_shadow = CheckPointShadow(pointLights[i].position, i, bias, FragPos);
		}
		
		Lo = CalcPoint(i, FragPos, viewdir, normal, mor, F0, albedo);
		
		color += max(((ambient * (0.1) + Lo * (point_shadow))), vec3(0.0)) * ssao;
	}
	 

	FragColor = color;
	BrightColor = BloomThresholdFilter(color, 1.0);
}