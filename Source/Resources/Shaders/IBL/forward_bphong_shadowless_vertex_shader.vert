#version 440 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 FragPos;
out vec4 FragPosLightSpace;


uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;
uniform mat4 directional_light_space_matrix;

out vec3 normal;

// ------------------

void main()
{
	//----------------------------------------------------------------------

	FragPos = vec3(model * vec4(aPos,1.0));
    TexCoords = aTexCoords;
	normal = aNormal;
	FragPosLightSpace = directional_light_space_matrix * vec4(FragPos,1.0);
	//----------------------------------------------------------------------


	gl_Position = projection * view * vec4(FragPos, 1.0);
}