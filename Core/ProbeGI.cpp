#include "ProbeGI.h"

#include "ShaderManager.h"

#include "ShadowMapHandler.h"

//#define LARGE_RANGE_PROBE_GI

namespace Lumen {
	namespace ProbeGI {

#ifndef LARGE_RANGE_PROBE_GI
		const int ProbeGridX = 48;
		const int ProbeGridY = 24;
		const int ProbeGridZ = 48;
		const glm::vec3 ProbeBoxSize = glm::vec3(24.0f, 12.0f, 24.0f);
#else 

		const int ProbeGridX = 64;
		const int ProbeGridY = 32;
		const int ProbeGridZ = 64;
		const glm::vec3 ProbeBoxSize = glm::vec3(32.0f, 16.0f, 32.0f);
#endif

		static GLuint _ProbeDataTextures[2] = { 0, 0 };
		static GLuint _PrevProbeDataTexture[2] = { 0, 0 };
		static GLuint _ProbeMapSSBO = 0;
		static glm::vec3 LastOrigin = glm::vec3(0.0f);
		static glm::vec3 _PreviousOrigin = glm::vec3(0.0f);
		static glm::uvec2 _CurrentDataTextures;
	}
}

void Lumen::ProbeGI::Initialize()
{
	glGenTextures(1, &_ProbeDataTextures[0]);
	glBindTexture(GL_TEXTURE_3D, _ProbeDataTextures[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);

	glGenTextures(1, &_ProbeDataTextures[1]);
	glBindTexture(GL_TEXTURE_3D, _ProbeDataTextures[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);

	glGenTextures(1, &_PrevProbeDataTexture[0]);
	glBindTexture(GL_TEXTURE_3D, _PrevProbeDataTexture[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);

	glGenTextures(1, &_PrevProbeDataTexture[1]);
	glBindTexture(GL_TEXTURE_3D, _PrevProbeDataTexture[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);

	// 8x8 luminance and depth/variance map
	glGenBuffers(1, &_ProbeMapSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ProbeMapSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, (ProbeGridX * ProbeGridY * ProbeGridZ) * sizeof(glm::vec2) * 8 * 8, nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

static float Align(float value, float size)
{
	return std::floor(value / size) * size;
}

void Lumen::ProbeGI::UpdateProbes(int Frame, RayIntersector<BVH::StacklessTraversalNode>& Intersector, CommonUniforms& uniforms, GLuint Skymap)
{
	GLClasses::ComputeShader& ProbeUpdate = ShaderManager::GetComputeShader("PROBE_UPDATE");

	bool Checkerboard = Frame % 2 == 0;
	GLuint CurrentVolumeTextures[2] = { Checkerboard ? _ProbeDataTextures[0] : _PrevProbeDataTexture[0], Checkerboard ? _ProbeDataTextures[1] : _PrevProbeDataTexture[1] };
	GLuint PreviousVolumeTextures[2] = { (!Checkerboard) ? _ProbeDataTextures[0] : _PrevProbeDataTexture[0], (!Checkerboard) ? _ProbeDataTextures[1] : _PrevProbeDataTexture[1] };

	_CurrentDataTextures.x = CurrentVolumeTextures[0];
	_CurrentDataTextures.y = CurrentVolumeTextures[1];

	ProbeUpdate.Use();

	_PreviousOrigin = LastOrigin;

	LastOrigin = glm::vec3(uniforms.InvView[3]);
	LastOrigin.x = Align(LastOrigin.x, 1.0f);
	LastOrigin.y = Align(LastOrigin.y, 1.0f);
	LastOrigin.z = Align(LastOrigin.z, 1.0f);

	ProbeUpdate.SetVector3f("u_SunDirection", uniforms.SunDirection);
	ProbeUpdate.SetVector3f("u_BoxOrigin", LastOrigin);
	ProbeUpdate.SetVector3f("u_Resolution", glm::vec3(ProbeGridX,ProbeGridY,ProbeGridZ));
	ProbeUpdate.SetVector3f("u_Size", ProbeBoxSize);
	ProbeUpdate.SetVector3f("u_PreviousOrigin", _PreviousOrigin);
	ProbeUpdate.SetFloat("u_Time", glfwGetTime());

	ProbeUpdate.SetInteger("u_Skymap", 4);

	ProbeUpdate.SetInteger("u_PreviousSHA", 5);
	ProbeUpdate.SetInteger("u_PreviousSHB", 6);

	for (int i = 0; i < 5; i++) {

		const int BindingPointStart = 8;

		std::string Name = "u_ShadowMatrices[" + std::to_string(i) + "]";
		std::string NameClip = "u_ShadowClipPlanes[" + std::to_string(i) + "]";
		std::string NameTex = "u_ShadowTextures[" + std::to_string(i) + "]";

		ProbeUpdate.SetMatrix4(Name, ShadowHandler::GetShadowViewProjectionMatrix(i));
		ProbeUpdate.SetInteger(NameTex, i + BindingPointStart);
		ProbeUpdate.SetFloat(NameClip, ShadowHandler::GetShadowCascadeDistance(i));

		glActiveTexture(GL_TEXTURE0 + i + BindingPointStart);
		glBindTexture(GL_TEXTURE_2D, ShadowHandler::GetShadowmap(i));
	}

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Skymap);

	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_3D, PreviousVolumeTextures[0]);

	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_3D, PreviousVolumeTextures[1]);

	glBindImageTexture(0, CurrentVolumeTextures[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);
	glBindImageTexture(1, CurrentVolumeTextures[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ProbeMapSSBO);

	Intersector.BindEverything(ProbeUpdate, uniforms.Frame < 128);

	glDispatchCompute(ProbeGridX / 8, ProbeGridY / 4, ProbeGridZ / 8);

	glUseProgram(0);
}

glm::vec3 Lumen::ProbeGI::GetProbeGridSize()
{
	return ProbeBoxSize;
}

glm::vec3 Lumen::ProbeGI::GetProbeGridRes()
{
	return glm::vec3(ProbeGridX, ProbeGridY, ProbeGridZ);
}

glm::vec3 Lumen::ProbeGI::GetProbeBoxOrigin()
{
	return LastOrigin;
}

GLuint Lumen::ProbeGI::GetProbeDataSSBO()
{
	return _ProbeMapSSBO;
}

glm::uvec2 Lumen::ProbeGI::GetProbeDataTextures()
{
	return _CurrentDataTextures;
}
