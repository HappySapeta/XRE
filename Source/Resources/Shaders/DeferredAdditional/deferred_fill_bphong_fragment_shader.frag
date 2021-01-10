#version 440 core

layout (location = 0) out vec4 FragColorOut;
layout (location = 1) out vec3 FragNormalOut;
layout (location = 2) out float FragDepthOut;

in vec2 TexCoords;
in vec3 FragPos;

in vec3 object_normal, object_tangent;

//-------------------------

uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D texture_normal;

vec3 TangentToWorldNormal(vec3 normal_from_texture)
{
    vec3 B  = normalize(cross(object_normal, object_tangent));
    mat3 TBN = mat3(object_tangent, B, object_normal);

	return normalize(TBN * normal_from_texture);
}

void main()
{
	vec4 diffuse_color = texture(texture_diffuse, TexCoords);

	if(diffuse_color.a <0.1)
	{
		discard;
	}

	FragColorOut = vec4(diffuse_color.rgb, texture(texture_specular, TexCoords).r);
	FragNormalOut = TangentToWorldNormal(normalize(texture(texture_normal, TexCoords).xyz * 2.0 - 1.0)) * 0.5 + 0.5;
	FragDepthOut = gl_FragCoord.z;
}