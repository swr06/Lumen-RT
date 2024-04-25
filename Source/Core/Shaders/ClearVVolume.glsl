#version 430 core
#define COMPUTE


layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(r8ui, binding = 0) uniform uimage3D o_Texture;

void main() { 
	imageStore(o_Texture, ivec3(gl_GlobalInvocationID.xyz), ivec4(0.0f));
}