#version 440 core

#define MAX_POINT_LIGHTS 20
#define MAX_POINT_SHADOWS 3

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

struct DirectionalLight {
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

in vec3 FragPos;  
in vec2 TexCoords;
in vec4 FragPosLightSpace;

//-------------------------

uniform DirectionalLight directionalLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

uniform int N_POINT;
uniform bool directional_lighting_enabled;
//-------------------------

uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D texture_normal;

// Shadow Textures -------------------------------
uniform sampler2D directional_shadow_depth_map;
uniform samplerCube point_shadow_depth_map[MAX_POINT_SHADOWS];

// ------------------------------------------------

uniform float shininess;
uniform vec3 camera_pos;
uniform float near;
uniform float far;
uniform vec3 directional_light_position;

// Tangent Space Data

in vec3 light_pos_tspace[6];
in vec3 camera_position_tspace;
in vec3 frag_pos_tspace;


//-------------------------
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

float chebyshevUpperBound(float d_blocker, vec2 d_recv)
{
	float p_max;

	if(d_blocker <= d_recv.x)
	{
		return 1.0;
	}

	float variance = d_recv.y - (d_recv.x * d_recv.x);
	variance = min(1.0f, max( 0.00001, variance) );

	float d = d_recv.x - d_blocker;
	p_max = variance / (variance + d * d);

	return ReduceLightBleeding(p_max, 1.0);
}

float CheckDirectionalShadow(float bias, vec3 lightpos, vec3 FragPos)
{		
	vec3 projCoords = FragPosLightSpace.xyz;// / FragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	vec2 closest_depth = texture(directional_shadow_depth_map, projCoords.xy, 0).rg;

	//vec4 closest_depth_r = textureGather(directional_shadow_depth_map, projCoords.xy, 0);
	//vec4 closest_depth_g = textureGather(directional_shadow_depth_map, projCoords.xy, 1);

	//vec2 avg_m = vec2((closest_depth_r.x + closest_depth_r.y + closest_depth_r.z + closest_depth_r.w) / 4.0, 
					  //(closest_depth_g.x + closest_depth_g.y + closest_depth_g.z + closest_depth_g.w) / 4.0);

	return 1.0 - chebyshevUpperBound(length(FragPos - lightpos) / far, closest_depth);
}

vec3 CalcDirectional(vec3 diffuse_texture_color,vec3 specular_texture_color,vec3 normal,vec3 camera_dir)
{
	float bias = 0.00001;
	//bias = max(0.001 * (1.0 - dot(normalize(fragment_normal), normalize(-directionalLight.direction))), 0.0001);

	float directional_shadow = 0.0;
	directional_shadow = CheckDirectionalShadow(bias, directional_light_position, FragPos);

	vec3 ambient = directionalLight.color * diffuse_texture_color; //ambient
	
	float diff = max(dot(normalize(normal), normalize(light_pos_tspace[0])),0.0);
	vec3 diffuse = directionalLight.color * diff * diffuse_texture_color; //diffuse
	
	vec3 halfway = normalize(normalize(light_pos_tspace[0]) + normalize(camera_dir));
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = directionalLight.color * spec * specular_texture_color; // specular

	return ambient * 0.01 + diffuse * 0.8 * (1.0 - directional_shadow) + specular * 1.0 * (1.0 - directional_shadow);
}

float CheckPointShadow(vec3 point_light_pos,int index, float bias, vec3 FragPos)
{
	vec3 light_to_frag = FragPos - point_light_pos;

	float current_depth = length(light_to_frag)/far;

	vec2 static_depth = texture(point_shadow_depth_map[index], normalize(light_to_frag)).rg;
	vec2 dynamic_depth = texture(point_shadow_depth_map[index], normalize(light_to_frag)).ba;
	
	vec2 closest_depth = static_depth.x < dynamic_depth.x ? static_depth : dynamic_depth; 

	return 1.0 - chebyshevUpperBound(current_depth,closest_depth);
}

vec3 CalcPoint(PointLight pl, const vec3 diffuse_texture_color, const vec3 specular_texture_color,const vec3 normal, const vec3 viewdir, const vec3 lightdir, int index)
{
	float bias = 0.001;
	//bias = max(0.01 * (1 - dot(normal, normalize(lightdir))), 0.001);

	float point_shadow = 0.0;
	if(index < MAX_POINT_SHADOWS)
	{
		point_shadow = CheckPointShadow(pl.position, index, bias, FragPos);
	}
	

	vec3 ambient = pl.color * diffuse_texture_color; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)), 0.0);
	vec3 diffuse = pl.color * diff * diffuse_texture_color; // diffuse
	
	vec3 halfway = normalize(lightdir + viewdir);
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = pl.color * spec * specular_texture_color; // specular
	
	float distance = length(pl.position - FragPos);
	float attenuation = 1.0/(pl.kc + pl.kl * distance + pl.kq * distance * distance);

	return ambient * 0.001 + diffuse * attenuation * 0.7 * (1.0 - point_shadow) + specular * attenuation * 0.9 * (1.0 - point_shadow);
}

void main()
{
    vec4 diffusetexture_sample =  texture(texture_diffuse, TexCoords);

	if(diffusetexture_sample.a < 0.1)
	{
		discard;
	}

	vec3 speculartexture_sample = vec3(texture(texture_specular, TexCoords).r);	
	vec3 tNormal = texture(texture_normal, TexCoords).rgb * 2.0 - 1.0;

	vec3 viewdir = normalize(camera_position_tspace - frag_pos_tspace);

	vec3 color = vec3(0.0);

	if(directional_lighting_enabled)
	{
		color = max(CalcDirectional(diffusetexture_sample.rgb, speculartexture_sample, tNormal, viewdir),vec3(0.0));
	}
	
	for(int i=0; i<N_POINT; i++)
	{
		vec3 lightDir = normalize(light_pos_tspace[1 + i] - frag_pos_tspace);
		color += max(CalcPoint(pointLights[i], diffusetexture_sample.rgb, speculartexture_sample, tNormal, viewdir, lightDir, i),vec3(0.0));
	}

	//color = tNormal;
	FragColor = vec4(color , 1.0);
	BrightColor = vec4(BloomThresholdFilter(color, 6.5), 1.0);
}