#version 330 core

#define N_POINT 1
#define N_SPOT 1

out vec4 FragColor;

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

in vec3 FragPos;  
in vec3 vNormal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

//-------------------------

uniform DirectionalLight directionalLight;
uniform PointLight pointLights[N_POINT];
uniform SpotLight spotLights[N_SPOT];

//-------------------------

uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D texture_normal;

// Shadow Textures -------------------------------
uniform int directional_shadow_textures_count;
uniform int point_shadow_textures_count;

uniform sampler2D shadow_depth_map_directional[1];
uniform samplerCube shadow_depth_map_cubemap[5];
// ------------------------------------------------

uniform float shininess;
uniform vec3 camera_pos;

// Tangent Space Data

in vec3 light_pos_tspace;
in vec3 camera_position_tspace;
in vec3 frag_pos_tspace;


//-------------------------
vec2 texelSize = vec2(1.0 / textureSize(shadow_depth_map_directional[0], 0));

vec2 poissonDisk[64];

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

vec3 restrictOffset(vec3 cubemap_direction, vec2 offset)
{
	vec3 face = vec3(0.0);
	float x = cubemap_direction.x;
	float y = cubemap_direction.y;
	float z = cubemap_direction.z;


	if(x>0 && fabs(x) >= fabs(y) && fabs(x) >= fabs(z))
	{
		face = vec3(1.0, offset.x, offset.y); // positive x
	}
	else if(x<0 && fabs(x)>= fabs(y) && fabs(x) >= fabs(z))
	{
		face = vec3(-1.0, offset.x, offset.y); // negative x
	}
	else if(y>0 && fabs(y)>= fabs(x) && fabs(y) >= fabs(z))
	{
		face = vec3(offset.x, 1.0, offset.y); // positive y
	}
	else if(y<0 && fabs(y)>= fabs(x) && fabs(y) >= fabs(z))
	{
		face = vec3(offset.x, -1.0, offset.y); // negative y
	}
	else if(z>0 && fabs(z)>= fabs(y) && fabs(z) >= fabs(x))
	{
		face = vec3(offset.x, offset.y, 1.0); // positive z
	}
	else if(z<0 && fabs(z)>= fabs(y) && fabs(z) >= fabs(x))
	{
		face = vec3(offset.x, offset.y, -1.0); // negative z
	}

	return face;
}

float CheckDirectionalShadow(float bias)
{	
	float shadow = 0.0;
	vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;


	for(int i=0;i<32; i++)
	{
		vec2 offset = poissonDisk[i] * (texelSize) * 2.0;
		if(projCoords.z - bias > texture(shadow_depth_map_directional[0], projCoords.xy + offset).r)
		{
			shadow += 1.0;
		}
	}

	shadow /= 32.0;

	return shadow;
}

vec3 CalcDirectional(vec3 diffuse_texture_color,vec3 specular_texture_color,vec3 normal,vec3 camera_dir)
{
	float bias = 0.00001;
	//bias = max(0.001 * (1.0 - dot(normalize(fragment_normal), normalize(-directionalLight.direction))), 0.0001);

	float directional_shadow = 0.0;
	//directional_shadow = CheckDirectionalShadow(bias);

	vec3 ambient = directionalLight.color * diffuse_texture_color; //ambient
	
	float diff = max(dot(normalize(normal), normalize(light_pos_tspace)),0.0);
	vec3 diffuse = directionalLight.color * diff * diffuse_texture_color; //diffuse
	
	vec3 halfway = normalize(normalize(light_pos_tspace) + normalize(camera_dir));
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = directionalLight.color * spec * specular_texture_color; // specular

	return ambient * 0.05 + diffuse * 0.8 * (1.0 - directional_shadow) + specular * 1.0 * (1.0 - directional_shadow);
}
float sampleShadow(int num_samples, float sample_spread,  float b, vec3 f_to_l, float d, bool stratified)
{
	float s = 0.0;
	vec3 restrictedOffset = vec3(0.0);
	int poisson_index;
	for(int i=0; i<num_samples; i++)
	{
		poisson_index = stratified ? int(num_samples*random(gl_FragCoord.xyy, i))%num_samples : i;
		restrictedOffset = restrictOffset(f_to_l, poissonDisk[poisson_index]) * sample_spread;
		s += d - b > texture(shadow_depth_map_cubemap[0], normalize(f_to_l + restrictedOffset)).r ? 1.0 : 0.0;
	}
	
	return s;
}
float CheckPointShadow(vec3 point_light_pos, int index, float bias)
{
	float shadow = 0.0;

	int penumbra_test_samples = 5;
	int pcf_samples = 16;

	vec3 frag_to_light = FragPos - point_light_pos;
	float current_depth = length(frag_to_light)/100.0;

	float blocker_depth = texture(shadow_depth_map_cubemap[0], frag_to_light).r;
	float pcss_mult = 2.0;
	float pcss = max(min(blocker_depth * blocker_depth * pcss_mult,0.1),0.0001);

	float p = sampleShadow(penumbra_test_samples, 0.02, bias, frag_to_light, current_depth, false);
	float ratio = p / penumbra_test_samples;
	
	float dulling = min((0.01/fabs(current_depth-blocker_depth)),1.0);

	if(ratio == 0)
	{
		shadow = 0.0;
	}
	else if(ratio==1.0)
	{
		shadow = 1.0;
	}
	else
	{
		shadow = sampleShadow(pcf_samples, pcss, bias, frag_to_light, current_depth, true);
		shadow /= pcf_samples;
	}

	return shadow * dulling;
}

vec3 CalcPoint(PointLight pl, const vec3 diffuse_texture_color, const vec3 specular_texture_color,const vec3 normal, const vec3 viewdir, const vec3 lightdir, int index)
{
	float bias = 0.001;
	//bias = max(0.01 * (1 - dot(normal, normalize(lightdir))), 0.001);

	float point_shadow = 1.0;
	point_shadow = CheckPointShadow(pl.position, index, bias);

	vec3 ambient = pl.color * diffuse_texture_color; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)), 0.0);
	vec3 diffuse = pl.color * diff * diffuse_texture_color; // diffuse
	
	vec3 halfway = normalize(lightdir + viewdir);
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = pl.color * spec * specular_texture_color; // specular
	
	float distance = length(pl.position - FragPos);
	float attenuation = 1.0/(pl.kc + pl.kl * distance + pl.kq * distance * distance);

	return ambient * 0.05 + diffuse * attenuation * 0.7 * (1.0 - point_shadow) + specular * attenuation * 0.9 * (1.0 - point_shadow);
}

vec3 CalcSpot(SpotLight sl, const vec3 diffuse_texture_color, const vec3 specular_texture_color, const vec3 normal, const vec3 viewdir, const vec3 lightdir)
{
	vec3 ambient = sl.color * diffuse_texture_color; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)), 0.0);
	vec3 diffuse = sl.color * diff * diffuse_texture_color; // diffuse
	
	vec3 halfway = normalize(lightdir + viewdir);
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = sl.color * spec * specular_texture_color; // specular
	
	float distance = length(sl.position - FragPos);
	float theta = dot(normalize(lightdir), normalize(-sl.direction));
	float epsilon = (sl.innercutoff - sl.outercutoff);
	
	float cutoffintensity = (theta - sl.outercutoff) / epsilon;
	float falloffintensity = 1/(sl.kc + sl.kl * distance + sl.kq * distance * distance);
	float intensity = clamp(cutoffintensity * falloffintensity, 0.0, 1.0);
	
	return ambient * 0.1 + diffuse * intensity * 0.7  + specular * intensity * 0.9;
}


void main()
{
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
	

    vec3 diffusetexture_sample =  texture(texture_diffuse, TexCoords).rgb;
	vec3 speculartexture_sample = texture(texture_specular, TexCoords).rgb;	
	vec3 tNormal = texture(texture_normal, TexCoords).rgb * 2.0 - 1.0;

	vec3 viewdir = normalize(camera_position_tspace - frag_pos_tspace);

	vec3 color = vec3(0.0);
	
	color = max(CalcDirectional(diffusetexture_sample, speculartexture_sample, tNormal, viewdir),vec3(0.0));
		
	for(int i=0; i<N_POINT; i++)
	{
		//color += max(CalcPoint(pointLights[i], diffusetexture_sample, speculartexture_sample,vNormal, viewdir, normalize(pointLights[i].position - FragPos), i),vec3(0.0));
	}
	
	for(int i=0; i<N_SPOT; i++)
	{
		//color += max(CalcSpot(spotLights[i], diffusetexture, speculartexture, Normal, viewdir, (spotLights[i].position - FragPos)),vec3(0.0));
	}
	FragColor = vec4(color, 1.0); 
} 