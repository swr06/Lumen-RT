#include "LightCuller.h"

#include "ModelRenderer.h"

#define LIGHTCULLGRIDRES 64

namespace Candela {

	static GLClasses::ComputeShader* ClearShader;
	static GLClasses::ComputeShader* CullGenShader;

	const float RangeV = 48;

	static GLuint CullVolumes[4];

	static GLuint AtomicIndicesSSBO = 0;

	static float Align(float value, float size)
	{
		return std::floor(value / size) * size;
	}

	static glm::vec3 SnapPosition(glm::vec3 p) {

		p.x = Align(p.x, 0.2f);
		p.y = Align(p.y, 0.2f);
		p.z = Align(p.z, 0.2f);

		return p;
	}


	void LightCuller::CreateVolumes()
	{

		for (int i = 0; i < 4; i++) {

			glGenTextures(1, &CullVolumes[i]);
			glBindTexture(GL_TEXTURE_3D, CullVolumes[i]);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, LIGHTCULLGRIDRES, LIGHTCULLGRIDRES, LIGHTCULLGRIDRES, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
		}
		
		ClearShader = new GLClasses::ComputeShader();
		ClearShader->CreateComputeShader("Core/Shaders/ClearCullVolume.glsl");
		ClearShader->Compile();

		CullGenShader = new GLClasses::ComputeShader();
		CullGenShader->CreateComputeShader("Core/Shaders/GenerateCullGrid.comp");
		CullGenShader->Compile();

		glGenBuffers(1, &AtomicIndicesSSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, AtomicIndicesSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 4 + (sizeof(int) * LIGHTCULLGRIDRES * LIGHTCULLGRIDRES * LIGHTCULLGRIDRES), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void LightCuller::GenerateVolume(glm::vec3 Position,  GLuint SphereLightSSBO, int NumberOfSphereLights)
	{
		glClearNamedBufferData(AtomicIndicesSSBO, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

		Position = SnapPosition(Position);

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glUseProgram(0);

		int GROUP_SIZE = 8;

		ClearShader->Use();
		glBindImageTexture(0, CullVolumes[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glBindImageTexture(1, CullVolumes[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glBindImageTexture(2, CullVolumes[2], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glBindImageTexture(3, CullVolumes[3], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glDispatchCompute(LIGHTCULLGRIDRES / GROUP_SIZE, LIGHTCULLGRIDRES / GROUP_SIZE, LIGHTCULLGRIDRES / GROUP_SIZE);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		
		CullGenShader->Use();

		CullGenShader->SetVector3f("u_Position", Position);
		CullGenShader->SetInteger("u_Res", LIGHTCULLGRIDRES);
		CullGenShader->SetInteger("u_LightCount", NumberOfSphereLights);
		CullGenShader->SetFloat("u_Size", RangeV);

		glBindImageTexture(0, CullVolumes[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glBindImageTexture(1, CullVolumes[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glBindImageTexture(2, CullVolumes[2], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glBindImageTexture(3, CullVolumes[3], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, AtomicIndicesSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, SphereLightSSBO);
		glDispatchCompute(LIGHTCULLGRIDRES / GROUP_SIZE, LIGHTCULLGRIDRES / GROUP_SIZE, LIGHTCULLGRIDRES / GROUP_SIZE);
		//glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	GLuint LightCuller::GetVolume(int i) {

		if (i > 3) {
			throw "bro :skull:";
		}

		return CullVolumes[i];
	}

	int LightCuller::GetVolSize()
	{
		return LIGHTCULLGRIDRES;
	}

	int LightCuller::GetVolRange()
	{
		return int(RangeV);
	}

	void LightCuller::RecompileShaders()
	{
		delete ClearShader;
		delete CullGenShader;

		ClearShader = new GLClasses::ComputeShader();
		ClearShader->CreateComputeShader("Core/Shaders/ClearCullVolume.glsl");
		ClearShader->Compile();

		CullGenShader = new GLClasses::ComputeShader();
		CullGenShader->CreateComputeShader("Core/Shaders/GenerateCullGrid.comp");
		CullGenShader->Compile();

	}

	GLuint LightCuller::GetLightIndicesSSBO()
	{
		return AtomicIndicesSSBO;
	}

}