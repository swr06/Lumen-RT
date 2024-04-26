#include "Voxelizer.h"

#include "ModelRenderer.h"

#define VOXELRES 64

namespace Candela {

	static GLClasses::Shader* VoxelizeShader;
	static GLClasses::ComputeShader* ClearShader;

	const float RangeV = 32;

	static GLuint VoxelMap = 0;

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


	void Voxelizer::CreateVolumes()
	{
		glGenTextures(1, &VoxelMap);
		glBindTexture(GL_TEXTURE_3D, VoxelMap);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, VOXELRES, VOXELRES, VOXELRES, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);


		ClearShader = new GLClasses::ComputeShader();
		ClearShader->CreateComputeShader("Core/Shaders/ClearVVolume.glsl");
		ClearShader->Compile();

		VoxelizeShader = new GLClasses::Shader();
		VoxelizeShader->CreateShaderProgramFromFile("Core/Shaders/VoxelizationVertex.glsl",
													"Core/Shaders/VoxelizationRadiance.glsl",
													"Core/Shaders/VoxelizationGeometry.geom");
		VoxelizeShader->CompileShaders();

	}

	void Voxelizer::Voxelize(glm::vec3 Position, const std::vector<Entity*>& EntityList)
	{

		Position = SnapPosition(Position);

		glBindTexture(GL_TEXTURE_3D, VoxelMap);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glUseProgram(0);

		int GROUP_SIZE = 8;

		ClearShader->Use();
		glBindImageTexture(0, VoxelMap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glDispatchCompute(VOXELRES / GROUP_SIZE, VOXELRES / GROUP_SIZE, VOXELRES / GROUP_SIZE);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Voxelize ->

		VoxelizeShader->Use();

		glBindTexture(GL_TEXTURE_3D, VoxelMap);
		glBindImageTexture(0, VoxelMap, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);

		VoxelizeShader->SetVector3f("u_VoxelGridCenter", Position);
		VoxelizeShader->SetVector3f("u_VoxelGridCenterF", Position);
		VoxelizeShader->SetVector3f("u_CoverageSize", glm::vec3(RangeV));
		VoxelizeShader->SetFloat("u_CoverageSizeF", (RangeV));
		VoxelizeShader->SetInteger("u_VolumeSize", VOXELRES);

		glViewport(0, 0, VOXELRES, VOXELRES);

		for (auto& e : EntityList) {

			if (e->m_EmissiveAmount > 0.001f) {
				continue;
			}
			RenderEntityV(*e, *VoxelizeShader);
		}
	}

	GLuint Voxelizer::GetVolume() {
		return VoxelMap;
	}

	int Voxelizer::GetVolSize()
	{
		return VOXELRES;
	}

	int Voxelizer::GetVolRange()
	{
		return int(RangeV);
	}

	void Voxelizer::RecompileShaders()
	{
		delete ClearShader;
		delete VoxelizeShader;


		ClearShader = new GLClasses::ComputeShader();
		ClearShader->CreateComputeShader("Core/Shaders/ClearVVolume.glsl");
		ClearShader->Compile();

		VoxelizeShader = new GLClasses::Shader();
		VoxelizeShader->CreateShaderProgramFromFile("Core/Shaders/VoxelizationVertex.glsl",
			"Core/Shaders/VoxelizationRadiance.glsl",
			"Core/Shaders/VoxelizationGeometry.geom");
		VoxelizeShader->CompileShaders();
	}


}