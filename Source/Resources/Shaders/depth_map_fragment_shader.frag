#version 440 core

uniform float farPlane;
uniform vec3 lightPos;

in vec4 FragPos;

void main()
{	
	float depth = length(FragPos.xyz - lightPos);
	depth /= farPlane;
	gl_FragDepth = depth;
}