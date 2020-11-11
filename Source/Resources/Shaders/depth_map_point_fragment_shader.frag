#version 440 core

uniform float farPlane;
uniform vec3 lightPos;

in vec4 FragPos;

out float depth;

void main()
{	

	depth = length(FragPos.xyz - lightPos)/farPlane;
/*
	float depth = length(lightPos - FragPos.xyz) / farPlane;
	depth = depth * 0.5 + 0.5;

	float m1 = depth;
	float m2 = depth * depth;

	float dx = dFdx(depth);
	float dy = dFdx(depth);

	m2 += 0.25 * (dx * dx + dy * dy);
*/
	//gl_FragColor = vec4(depth, 0.0, 0.0, 0.0);
}