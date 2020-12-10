#version 440 core

in vec4 FragPos;

uniform float farPlane;
uniform vec3 lightPos;
uniform int mode;

out vec4 depth;

void main()
{	
	float d = length(FragPos.xyz - lightPos) / farPlane;

	float m1 = d;
	float m2 = d * d;

	float dx = dFdx(depth.x);
	float dy = dFdx(depth.y);

	m2 += 0.25 * (dx * dx + dy * dy);

	if(mode == 0) // static
	{
		depth.r = m1;
		depth.g = m2;
	}
	else if(mode == 1) // dynamic
	{
		depth.b = m1;
		depth.a = m2;
	}
}