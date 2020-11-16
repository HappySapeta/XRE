#version 440 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

uniform bool horizontal;
float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
	vec2 tex_offset = 2.0 / textureSize(screenTexture,0);
	vec3 result = texture(screenTexture, TexCoords).rgb * weights[0];

	if(horizontal)
	{
		for(int i=1; i<5; i++)
		{
			result += texture(screenTexture, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
			result += texture(screenTexture, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weights[i];
		}
	}
	else
	{
		for(int i=1; i<5; i++)
		{
			result += texture(screenTexture, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weights[i];
			result += texture(screenTexture, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weights[i];
		}
	}

	FragColor = vec4(result, 1.0);
}