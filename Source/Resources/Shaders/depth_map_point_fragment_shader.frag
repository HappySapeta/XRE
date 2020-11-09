#version 440 core

uniform float farPlane;
uniform vec3 lightPos;

in vec4 FragPos;
float depth;

void main()
{	
	depth = length(FragPos.xyz - lightPos)/farPlane;
	gl_FragDepth = depth;
}