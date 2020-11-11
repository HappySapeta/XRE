#version 440 core

#define N_POINT 3
#define N_SPOT 1

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

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

struct SpotLight{
	vec3 position;
	vec3 direction;
	
	vec3 color;
	
	float innercutoff;
	float outercutoff;
	
	float kc;
	float kl;
	float kq;
};
// ------------------------

vec3 FragPos;
vec4 FragPosLightSpace;
in vec2 TexCoords;

// Shadow Textures -------------------------------
uniform sampler2D shadow_depth_map_directional;

uniform samplerCube point_shadow_framebuffer_depth_color_texture_static[5];
uniform samplerCube point_shadow_framebuffer_depth_color_texture_dynamic[5];
// -----------------------

vec2 texelSize = vec2(1.0 / textureSize(shadow_depth_map_directional, 0));
vec2 poissonDisk[64];
float ambient_occlusion;

// -----------------------

uniform DirectionalLight directionalLight;
uniform PointLight pointLights[N_POINT]; // I forgot to mentiom 'uniform' and kept wandering for an hour why the screen would render black.
uniform SpotLight spotLights[N_SPOT];

uniform mat4 directional_light_space_matrix;
uniform bool directional_lighting_enabled;

// -----------------------



uniform float near;
uniform float far;

// -----------------------

uniform float shininess = 128.0;
uniform vec3 camera_pos;

uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D position_texture;
uniform sampler2D model_normal_texture;
uniform sampler2D tangent_texture;
uniform sampler2D texture_normal_texture;
uniform sampler2D ssao_texture;

float random(vec3 seed,int i)
{
	vec4 seed4 = vec4(seed, i);
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
	return fract(sin(dot_product) * 43758.5453);
}

float fabs(float v)
{
	return v < 0 ? -v : v;
}


float sampleDirectionalShadow(vec2 projCoords,float bias, float current_depth)
{
	float s = 0.0;

	float closest_depth = texture(shadow_depth_map_directional, projCoords).r;
	s = current_depth - bias > closest_depth ? 1.0 : 0.0;
	
	return s;
}

float CheckDirectionalShadow(float bias, vec3 lightpos, vec3 FragPos)
{	
	float shadow = 0.0;
	
	vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	vec3 light_to_frag = FragPos - lightpos;
	float current_depth = length(light_to_frag);

	shadow = sampleDirectionalShadow(projCoords.xy, bias, current_depth / far);

	return shadow;
}

vec3 CalcDirectional(vec3 diffuse_texture_color,float specular_texture_value,vec3 normal,vec3 viewdir, vec3 lightdir, vec3 FragPos)
{
	float bias = 0.00001;
	//bias = max(0.001 * (1.0 - dot(normalize(fragment_normal), normalize(-directionalLight.direction))), 0.0001);

	float directional_shadow = 0.0;
	directional_shadow = CheckDirectionalShadow(bias, directionalLight.position, FragPos);

	vec3 ambient = directionalLight.color * diffuse_texture_color * ambient_occlusion; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)),0.0);
	vec3 diffuse = directionalLight.color * diff * diffuse_texture_color; //diffuse
	
	vec3 halfway = normalize(normalize(lightdir) + normalize(viewdir));
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = directionalLight.color * spec * specular_texture_value; // specular

	return (ambient * 0.01 + diffuse * 0.8 * (1.0 - directional_shadow) + specular * 1.0 * (1.0 - directional_shadow)) * ambient_occlusion;
}

float chebyshevUpperBound(float dist, vec2 moments)
{
	float p_max;

	if(dist <= moments.x)
	{
		return 1.0;
	}

	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.00002);

	float d = dist - moments.x;
	p_max = variance / (variance + d * d);

	return p_max;
}

float samplePointShadow(float bias, float current_depth, vec3 light_to_frag_direction,int shadow_map_index)
{
	float s = 0.0;

	float static_depth, dynamic_depth;
	float closest_depth;

	static_depth = texture(point_shadow_framebuffer_depth_color_texture_static[shadow_map_index], normalize(light_to_frag_direction)).r;
	dynamic_depth = texture(point_shadow_framebuffer_depth_color_texture_dynamic[shadow_map_index], normalize(light_to_frag_direction)).r;

	closest_depth = static_depth < dynamic_depth ? static_depth : dynamic_depth;
	s += current_depth - bias > closest_depth ? 1.0 : 0.0;

	return s;
}
float CheckPointShadow(vec3 point_light_pos,int index, float bias, vec3 FragPos)
{
	float shadow = 0.0;

	vec3 light_to_frag = FragPos - point_light_pos;

	float current_depth = length(light_to_frag);
	vec3 light_to_frag_direction = normalize(light_to_frag);

	shadow = samplePointShadow(bias,current_depth / far, light_to_frag_direction, index);

	return shadow;
}

vec3 CalcPoint(PointLight pl,vec3 diffuse_texture_color, float specular_texture_value,vec3 normal, vec3 FragPos ,vec3 viewdir, vec3 lightdir,int index)
{
	float bias = 0.0001;
	//bias = max(0.01 * (1 - dot(normal, normalize(lightdir))), 0.001);

	float point_shadow = 0.0;
	point_shadow = CheckPointShadow(pl.position, index, bias, FragPos);

	vec3 ambient = pl.color * diffuse_texture_color * ambient_occlusion; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)), 0.0);
	vec3 diffuse = pl.color * diff * diffuse_texture_color ; // diffuse
	
	vec3 halfway = normalize(lightdir + viewdir);
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = pl.color * spec * vec3(specular_texture_value); // specular
	
	float distance = length(pl.position - FragPos);
	float attenuation = 1.0/(pl.kc + pl.kl * distance + pl.kq * distance * distance);

	return (ambient * 0.001 + diffuse * attenuation * 0.7 * (1.0 - point_shadow) + specular * attenuation * 0.9 * (1.0 - point_shadow));
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

void main()
{
	vec3 color = vec3(0.0);

	vec3 diffuse_texture_color = texture(diffuse_texture, TexCoords).rgb;
	float specular_texture_value = texture(specular_texture, TexCoords).r;
	FragPos = texture(position_texture, TexCoords).xyz;
	vec3 normal_from_texture = texture(texture_normal_texture, TexCoords).xyz * 2.0 - 1.0;
	ambient_occlusion = texture(ssao_texture, TexCoords).r;

	// T space
	// ------------------------------------------------------------------------
	vec3 N = texture(model_normal_texture, TexCoords).rgb;
	vec3 T = texture(tangent_texture, TexCoords).rgb;
	T = normalize(-dot(T, N) * N + T);
	
	vec3 B = cross(N, T);
	mat3 TBN = transpose(mat3(T,B,N));

	vec3 FragPos_tspace = TBN * FragPos;
	// ------------------------------------------------------------------------

	FragPosLightSpace = directional_light_space_matrix * vec4(FragPos,1.0);

	vec3 viewdir = vec3(0.0);
	vec3 lightdir = vec3(0.0);

	if(directional_lighting_enabled)
	{
		lightdir = TBN * directionalLight.position;
		viewdir = TBN * camera_pos - FragPos_tspace;

		color += max(CalcDirectional(diffuse_texture_color, specular_texture_value, normal_from_texture, viewdir, lightdir, FragPos),vec3(0.0));
	}

	for(int i=0; i<3; ++i)
	{
		lightdir = TBN * pointLights[i].position - FragPos_tspace;
		viewdir = TBN * camera_pos - FragPos_tspace;

		color += max(CalcPoint(pointLights[i],diffuse_texture_color, specular_texture_value, normal_from_texture, FragPos, viewdir, lightdir, i),vec3(0.0));
	}

	//color = vec3(ambient_occlusion);
	BrightColor = vec4(BloomThresholdFilter(vec3(0.0), 6.5), 1.0);
	FragColor = vec4(color, 1.0);
}