#version 440 core

layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D bloomTexture;

uniform bool gamma_correct = true;
uniform float gamma = 2.2;
uniform float exposure = 1.0;

vec3 color_sample;
vec3 out_color;

vec3 applyHDR(vec3 in_color)
{
	//return vec3(1.0) - exp(-in_color * exposure);
	return in_color / (in_color + vec3(1.0));
}

vec3 applyGammaCorrection(vec3 in_color)
{
	return pow(in_color, vec3(1.0/gamma));
}

void main()
{	
	color_sample = texture(screenTexture, TexCoords).rgb;
	vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;

	color_sample += bloomColor;
	// HDR
	// reinhard tone mapping
	out_color = applyHDR(color_sample);
	out_color = applyGammaCorrection(out_color);

	FragColor = vec4(out_color, 1.0);
}