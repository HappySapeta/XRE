#version 440 core

layout (location = 0) in vec3 aPos;

uniform float farPlane;
uniform mat4 model;
uniform mat4 light_view;
uniform bool back;

out vec4 FragWorldPos;
out float far;

void main()
{
	FragWorldPos = model * vec4(aPos, 1.0);
	gl_Position = light_view * FragWorldPos;

	highp float L = -gl_Position.z;

	gl_Position.xyz = normalize(gl_Position.xyz);
	gl_Position.xy /= 1.0 - gl_Position.z;

	gl_Position.z = ((1.0 - L) / (1.0 - farPlane)) * 2.0 - 1.0;
	gl_Position.w = 1.0;

	gl_Position.x /= 2;

	if(back)
	{
		gl_Position.x += 0.5;
	}
	else
	{
		gl_Position.x -= 0.5;
	}

	far = farPlane;
}