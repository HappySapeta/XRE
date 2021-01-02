#version 440 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec2 TexCoords;

out vec3 object_normal, object_tangent;

uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;

// ------------------

void main()
{	
	mat3 normalMatrix = transpose(inverse(mat3(model)));
	mat3 normalMatrix_view = mat3(model);

	object_tangent = normalize(vec3(normalMatrix * aTangent));
	object_normal = normalize(vec3(normalMatrix * aNormal));
	object_tangent = normalize(object_tangent - dot(object_tangent, object_normal) * object_normal);

	//----------------------------------------------------------------------

    TexCoords = aTexCoords;

	//----------------------------------------------------------------------
	gl_Position = projection * view * model * vec4(aPos, 1.0);
}