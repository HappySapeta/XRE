#version 440 core

layout (location = 0) out vec4 FragColorOut;
layout (location = 1) out float SpecularIntensityOut;
layout (location = 2) out vec3 FragModelNormalOut;
layout (location = 3) out vec3 FragTangentOut;
layout (location = 4) out vec3 FragTextureNormalOut;
layout (location = 5) out vec3 FragPosOut;

in vec2 TexCoords;
in vec3 FragPos;

in vec3 T;
in vec3 N;

//-------------------------

uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D texture_normal;

void main()
{
	vec4 diffuse_color = texture(texture_diffuse, TexCoords);

	if(diffuse_color.a <0.1)
	{
		discard;
	}

	FragColorOut = vec4(diffuse_color.rgb, 1.0);
	SpecularIntensityOut = texture(texture_specular, TexCoords).r;
	FragModelNormalOut = N;
	FragTangentOut = T;
	FragTextureNormalOut = texture(texture_normal, TexCoords).rgb;

	FragPosOut = FragPos;
}