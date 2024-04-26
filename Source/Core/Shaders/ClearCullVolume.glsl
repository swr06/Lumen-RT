#version 430 core
#define COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(r8ui, binding = 0) uniform uimage3D o_Texture;
layout(r8ui, binding = 1) uniform uimage3D o_Texture1;
layout(r8ui, binding = 2) uniform uimage3D o_Texture2;
layout(r8ui, binding = 3) uniform uimage3D o_Texture3;

void main() { 
	imageStore(o_Texture, ivec3(gl_GlobalInvocationID.xyz), ivec4(0.0f));
	imageStore(o_Texture1, ivec3(gl_GlobalInvocationID.xyz), ivec4(0.0f));
	imageStore(o_Texture2, ivec3(gl_GlobalInvocationID.xyz), ivec4(0.0f));
	imageStore(o_Texture3, ivec3(gl_GlobalInvocationID.xyz), ivec4(0.0f));
}