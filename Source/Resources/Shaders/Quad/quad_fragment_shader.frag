#version 440 core

layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D bloomTexture;

uniform bool gamma_correct = true;
uniform float gamma = 2.2;
uniform float a = 0.6;
uniform float b = 0.45333;
vec3 color_sample;
vec3 out_color;

float ToneMap(float x)
{
	return x <= a ? x : min(1, a + b - (b*b/(x - a + b)));
}

vec3 HDRtoLDR(vec3 in_color)
{
	return vec3(ToneMap(in_color.r), ToneMap(in_color.g), ToneMap(in_color.b));
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
	out_color = HDRtoLDR(color_sample);
	out_color = applyGammaCorrection(out_color);

	FragColor = vec4(out_color, 1.0);
}