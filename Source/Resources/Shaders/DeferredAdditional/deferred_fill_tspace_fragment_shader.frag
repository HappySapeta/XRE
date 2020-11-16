#version 440 core

layout (location = 0) out vec3 light_pos_tspace_out_0;
layout (location = 1) out vec3 light_pos_tspace_out_1;
layout (location = 2) out vec3 light_pos_tspace_out_2;
layout (location = 3) out vec3 light_pos_tspace_out_3;
layout (location = 4) out vec3 light_pos_tspace_out_4;
layout (location = 5) out vec3 light_pos_tspace_out_5;
layout (location = 6) out vec3 model_normal_out;

in vec3 light_pos_tspace_0;
in vec3 light_pos_tspace_1;
in vec3 light_pos_tspace_2;
in vec3 light_pos_tspace_3;
in vec3 light_pos_tspace_4;
in vec3 light_pos_tspace_5;
in vec3 model_normal;

void main()
{
	light_pos_tspace_out_0 = light_pos_tspace_0;
	light_pos_tspace_out_1 = light_pos_tspace_1;
	light_pos_tspace_out_2 = light_pos_tspace_2;
	light_pos_tspace_out_3 = light_pos_tspace_3;
	light_pos_tspace_out_4 = light_pos_tspace_4;
	light_pos_tspace_out_5 = light_pos_tspace_5;
	model_normal_out = model_normal;
}