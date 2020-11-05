#version 440 core

#define N_POINT 3
#define N_SPOT 1

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

vec4 FragPosLightSpace;
in vec2 TexCoords;

// Shadow Textures -------------------------------
uniform sampler2D shadow_depth_map_directional;

// -----------------------

vec2 texelSize = vec2(1.0 / textureSize(shadow_depth_map_directional, 0));
vec2 poissonDisk[64];

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

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

uniform float shininess = 128.0;
uniform vec3 camera_pos;

uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D position_texture;
uniform sampler2D model_normal_texture;
uniform sampler2D tangent_texture;
uniform sampler2D texture_normal_texture;

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

float sampleDirectionalShadow(int num_samples, float sample_spread,  vec2 projCoords,float bias, float current_depth,bool stratified)
{
	float s = 0.0;

	int poisson_index;

	for(int i=0; i<num_samples; i++)
	{
		poisson_index = stratified ? int(num_samples*random(gl_FragCoord.xyz, i))%num_samples : i;

		vec2 offset = poissonDisk[poisson_index] * (texelSize) * sample_spread;
		float closest_depth = texture(shadow_depth_map_directional, projCoords + offset).r;

		s += current_depth - bias > closest_depth ? 1.0 : 0.0;
	}
	
	return s;
}

float CheckDirectionalShadow(float bias, vec3 lightpos, vec3 FragPos)
{	
	float shadow = 0.0;
	
	int pcf_samples = 16;
	int penumbra_test_samples = 8;

	vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	vec3 light_to_frag = FragPos - lightpos;

	float current_depth = length(light_to_frag);
	vec3 light_to_frag_direction = normalize(light_to_frag);

	float count = sampleDirectionalShadow(penumbra_test_samples, 8.0, projCoords.xy ,bias, current_depth / far, false);
	
	if(count == 0.0)
	{
		shadow = 0.0;
	}
	else if(count == penumbra_test_samples)
	{
		shadow = 1.0;
	}
	else
	{
		shadow = sampleDirectionalShadow(pcf_samples, 2.0, projCoords.xy, bias, current_depth / far, true);
		shadow /= pcf_samples;
	}

	return shadow;
}

vec3 CalcDirectional(vec3 diffuse_texture_color,float specular_texture_value,vec3 normal,vec3 viewdir, vec3 lightdir, vec3 FragPos)
{
	float bias = 0.00001;
	//bias = max(0.001 * (1.0 - dot(normalize(fragment_normal), normalize(-directionalLight.direction))), 0.0001);

	float directional_shadow = 0.0;
	directional_shadow = CheckDirectionalShadow(bias, directionalLight.position, FragPos);

	vec3 ambient = directionalLight.color * diffuse_texture_color; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)),0.0);
	vec3 diffuse = directionalLight.color * diff * diffuse_texture_color; //diffuse
	
	vec3 halfway = normalize(normalize(lightdir) + normalize(viewdir));
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = directionalLight.color * spec * specular_texture_value; // specular

	return ambient * 0.01 + diffuse * 0.8 * (1.0 - directional_shadow) + specular * 1.0 * (1.0 - directional_shadow);
}

vec3 CalcPoint(PointLight pl, vec3 diffuse_texture_color, float specular_texture_value,vec3 normal, vec3 FragPos ,vec3 viewdir, vec3 lightdir, bool vertex_normal,int index)
{
	float point_shadow = 0.0;

	vec3 ambient = pl.color * diffuse_texture_color; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)), 0.0);
	vec3 diffuse = pl.color * diff * diffuse_texture_color; // diffuse
	
	vec3 halfway = normalize(lightdir + viewdir);
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = pl.color * spec * vec3(specular_texture_value); // specular
	
	float distance = length(pl.position - FragPos);
	float attenuation = 1.0/(pl.kc + pl.kl * distance + pl.kq * distance * distance);

	return ambient * 0.001 + diffuse * attenuation * 0.7 * (1.0 - point_shadow) + specular * attenuation * 0.9 * (1.0 - point_shadow);
}

void main()
{
	// over here, i am just assuming that the GPU creates just one copy for each of these samples, and they are allocated or deallocated just once, but i am probably wrong.
	poissonDisk[46] = vec2(0.001801, 0.780328);poissonDisk[15] = vec2(0.039766, -0.396100);poissonDisk[31] = vec2(0.034211, 0.979980);poissonDisk[0] = vec2(-0.613392, 0.617481);
	poissonDisk[47] = vec2(0.145177, -0.898984);poissonDisk[16] = vec2(0.751946, 0.453352);poissonDisk[32] = vec2(0.503098, -0.308878);poissonDisk[1] = vec2(0.170019, -0.040254);
	poissonDisk[48] = vec2(0.062655, -0.611866);poissonDisk[17] = vec2(0.078707, -0.715323);poissonDisk[33] = vec2(-0.016205, -0.872921);poissonDisk[2] = vec2(-0.299417, 0.791925);
	poissonDisk[49] = vec2(0.315226, -0.604297);poissonDisk[18] = vec2(-0.075838, -0.529344);poissonDisk[34] = vec2(0.385784, -0.393902);poissonDisk[3] = vec2(0.645680, 0.493210);
	poissonDisk[50] = vec2(-0.780145, 0.486251);poissonDisk[19] = vec2(0.724479, -0.580798);poissonDisk[35] = vec2(-0.146886, -0.859249);poissonDisk[4] = vec2(-0.651784, 0.717887);
	poissonDisk[51] = vec2(-0.371868, 0.882138);poissonDisk[20] = vec2(0.222999, -0.215125);poissonDisk[36] = vec2(0.643361, 0.164098);poissonDisk[5] = vec2(0.421003, 0.027070);
	poissonDisk[52] = vec2(0.200476, 0.494430);poissonDisk[21] = vec2(-0.467574, -0.405438);poissonDisk[37] = vec2(0.634388, -0.049471);poissonDisk[6] = vec2(-0.817194, -0.271096);
	poissonDisk[53] = vec2(-0.494552, -0.711051);poissonDisk[22] = vec2(-0.248268, -0.814753);poissonDisk[38] = vec2(-0.688894, 0.007843);poissonDisk[7] = vec2(-0.705374, -0.668203);
	poissonDisk[54] = vec2(0.612476, 0.705252);poissonDisk[23] = vec2(0.354411, -0.887570);poissonDisk[39] = vec2(0.464034, -0.188818);poissonDisk[8] = vec2(0.977050, -0.108615);
	poissonDisk[55] = vec2(-0.578845, -0.768792);poissonDisk[24] = vec2(0.175817, 0.382366);poissonDisk[40] = vec2(-0.440840, 0.137486);poissonDisk[9] = vec2(0.063326, 0.142369);
	poissonDisk[56] = vec2(-0.772454, -0.090976);poissonDisk[25] = vec2(0.487472, -0.063082);poissonDisk[41] = vec2(0.364483, 0.511704);poissonDisk[10] = vec2(0.203528, 0.214331);
	poissonDisk[57] = vec2(0.504440, 0.372295);poissonDisk[26] = vec2(-0.084078, 0.898312);poissonDisk[42] = vec2(0.034028, 0.325968);poissonDisk[11] = vec2(-0.667531, 0.326090);
	poissonDisk[58] = vec2(0.155736, 0.065157);poissonDisk[27] = vec2(0.488876, -0.783441);poissonDisk[43] = vec2(0.099094, -0.308023);poissonDisk[12] = vec2(-0.098422, -0.295755);
	poissonDisk[59] = vec2(0.391522, 0.849605);poissonDisk[28] = vec2(0.470016, 0.217933);poissonDisk[44] = vec2(0.693960, -0.366253);poissonDisk[13] = vec2(-0.885922, 0.215369);
	poissonDisk[60] = vec2(-0.620106, -0.328104);poissonDisk[29] = vec2(-0.696890, -0.549791);poissonDisk[45] = vec2(0.678884, -0.204688);poissonDisk[14] = vec2(0.566637, 0.605213);
	poissonDisk[61] = vec2(0.789239, -0.419965);poissonDisk[30] = vec2(-0.149693, 0.605762);poissonDisk[62] = vec2(-0.545396, 0.538133);poissonDisk[63] = vec2(-0.178564, -0.596057);
	


	vec3 color = vec3(0.0);

	vec3 diffuse_texture_color = texture(diffuse_texture, TexCoords).rgb;
	float specular_texture_value = texture(specular_texture, TexCoords).r;
	vec3 FragPos = texture(position_texture, TexCoords).rgb;
	vec3 normal_from_texture = texture(texture_normal_texture, TexCoords).rgb * 2.0 - 1.0;

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

		color += max(CalcPoint(pointLights[i], diffuse_texture_color, specular_texture_value, normal_from_texture, FragPos, viewdir, lightdir, false, i),vec3(0.0));
	}

	//color = normal;
	BrightColor = vec4(0.0);
	FragColor = vec4(color, 1.0);
}