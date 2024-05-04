#include "ProbeGI.h"

#include "ShaderManager.h"

#include "ShadowMapHandler.h"



namespace Candela {
	namespace ProbeGI {

		static GLuint _ProbeDataTextures[2] = { 0, 0 };
		static GLuint _PrevProbeDataTextures[2] = { 0, 0 };
		static GLuint _PrevFrameDataTextures[2] = { 0, 0 };
		static GLuint _ProbeMapSSBO = 0;
		static glm::vec3 LastOrigin = glm::vec3(0.0f);
		static glm::vec3 _PreviousOrigin = glm::vec3(0.0f);
		static glm::uvec3 _CurrentDataTextures;
		static GLuint _ProbeRawRadianceBuffers[2]; // <- Unprojected radiance

		// Scene voxel representation
		//static GLuint VoxelVolume = 0;
	}
}

void Candela::ProbeGI::Initialize()
{
	const int ProbeGridX = PROBE_GRID_X;
	const int ProbeGridY = PROBE_GRID_Y;
	const int ProbeGridZ = PROBE_GRID_Z;

	glGenTextures(1, &_ProbeDataTextures[0]);
	glBindTexture(GL_TEXTURE_3D, _ProbeDataTextures[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ);
	glClearTexImage(_ProbeDataTextures[0], 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, nullptr);

	glGenTextures(1, &_ProbeDataTextures[1]);
	glBindTexture(GL_TEXTURE_3D, _ProbeDataTextures[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ);
	glClearTexImage(_ProbeDataTextures[1], 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, nullptr);

	glGenTextures(1, &_PrevProbeDataTextures[0]);
	glBindTexture(GL_TEXTURE_3D, _PrevProbeDataTextures[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ);
	glClearTexImage(_PrevProbeDataTextures[0], 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, nullptr);

	glGenTextures(1, &_PrevProbeDataTextures[1]);
	glBindTexture(GL_TEXTURE_3D, _PrevProbeDataTextures[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ);
	glClearTexImage(_PrevProbeDataTextures[1], 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, nullptr);

	glGenTextures(1, &_PrevFrameDataTextures[0]);
	glBindTexture(GL_TEXTURE_3D, _PrevFrameDataTextures[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ);
	glClearTexImage(_PrevFrameDataTextures[0], 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, nullptr);

	glGenTextures(1, &_PrevFrameDataTextures[1]);
	glBindTexture(GL_TEXTURE_3D, _PrevFrameDataTextures[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32UI, ProbeGridX, ProbeGridY, ProbeGridZ);
	glClearTexImage(_PrevFrameDataTextures[1], 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, nullptr);

	// Raw radiance buffers
	glGenTextures(1, &_ProbeRawRadianceBuffers[0]);
	glBindTexture(GL_TEXTURE_3D, _ProbeRawRadianceBuffers[0]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R11F_G11F_B10F, ProbeGridX, ProbeGridY, ProbeGridZ);
	glClearTexImage(_ProbeRawRadianceBuffers[0], 0, GL_RGB, GL_FLOAT, nullptr);

	glGenTextures(1, &_ProbeRawRadianceBuffers[1]);
	glBindTexture(GL_TEXTURE_3D, _ProbeRawRadianceBuffers[1]);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R11F_G11F_B10F, ProbeGridX, ProbeGridY, ProbeGridZ);
	glClearTexImage(_ProbeRawRadianceBuffers[1], 0, GL_RGB, GL_FLOAT, nullptr);

	// Voxel volume
	//glGenTextures(1, &VoxelVolume);
	//glBindTexture(GL_TEXTURE_3D, VoxelVolume);
	//glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	//glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, VOXEL_GRID_X , VOXEL_GRID_Y, VOXEL_GRID_Z, 0, GL_RGBA, GL_FLOAT, nullptr);


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

void Candela::ProbeGI::UpdateProbes(int Frame, RayIntersector<BVH::StacklessTraversalNode>& Intersector, CommonUniforms& uniforms, GLuint Skymap, bool Temporal, int MaxFrames)
{
	const int ProbeGridX = PROBE_GRID_X;
	const int ProbeGridY = PROBE_GRID_Y;
	const int ProbeGridZ = PROBE_GRID_Z;

	GLClasses::ComputeShader& ProbeUpdate = ShaderManager::GetComputeShader("PROBE_UPDATE");
	GLClasses::ComputeShader& CopyVolume = ShaderManager::GetComputeShader("COPY_VOLUME");

	bool Checkerboard = Frame % 2 == 0;
	GLuint CurrentVolumeTextures[2] = { Checkerboard ? _ProbeDataTextures[0] : _PrevProbeDataTextures[0], Checkerboard ? _ProbeDataTextures[1] : _PrevProbeDataTextures[1] };
	GLuint PreviousVolumeTextures[2] = { (!Checkerboard) ? _ProbeDataTextures[0] : _PrevProbeDataTextures[0], (!Checkerboard) ? _ProbeDataTextures[1] : _PrevProbeDataTextures[1] };

	GLuint CurrentRawRadianceTexture = Checkerboard ? _ProbeRawRadianceBuffers[0] : _ProbeRawRadianceBuffers[1];
	GLuint PreviousRawRadianceTexture = Checkerboard ? _ProbeRawRadianceBuffers[1] : _ProbeRawRadianceBuffers[0];

	_CurrentDataTextures.x = CurrentVolumeTextures[0];
	_CurrentDataTextures.y = CurrentVolumeTextures[1];
	_CurrentDataTextures.z = CurrentRawRadianceTexture;

	ProbeUpdate.Use();

	_PreviousOrigin = LastOrigin;

	LastOrigin = glm::vec3(uniforms.InvView[3]);
	LastOrigin.x = Align(LastOrigin.x, 1.0f);
	LastOrigin.y = Align(LastOrigin.y, 1.0f);
	LastOrigin.z = Align(LastOrigin.z, 1.0f);

	ProbeUpdate.SetVector3f("u_SunDirection", uniforms.SunDirection);
	ProbeUpdate.SetVector3f("u_BoxOrigin", LastOrigin);
	ProbeUpdate.SetVector3f("u_Resolution", glm::vec3(ProbeGridX,ProbeGridY,ProbeGridZ));
	ProbeUpdate.SetVector3f("u_Size", PROBE_GRID_SIZE);
	ProbeUpdate.SetVector3f("u_PreviousOrigin", _PreviousOrigin);
	ProbeUpdate.SetFloat("u_Time", glfwGetTime());

	ProbeUpdate.SetInteger("u_Skymap", 4);

	ProbeUpdate.SetInteger("u_PreviousSHA", 5);
	ProbeUpdate.SetInteger("u_PreviousSHB", 6);
	ProbeUpdate.SetFloat("u_MaxFrames", MaxFrames);
	ProbeUpdate.SetBool("u_Temporal", Temporal);

	for (int i = 0; i < 5; i++) {

		const int BindingPointStart = 8;

		std::string Name = "u_ShadowMatrices[" + std::to_string(i) + "]";
		std::string NameClip = "u_ShadowClipPlanes[" + std::to_string(i) + "]";
		std::string NameTex = "u_ShadowTextures[" + std::to_string(i) + "]";

		ProbeUpdate.SetMatrix4(Name, ShadowHandler::GetShadowViewProjectionMatrix(i));
		ProbeUpdate.SetInteger(NameTex, i + BindingPointStart);
		ProbeUpdate.SetFloat(NameClip, ShadowHandler::GetShadowCascadeDistance(i));

		glActiveTexture(GL_TEXTURE0 + i + BindingPointStart);
		glBindTexture(GL_TEXTURE_2D, ShadowHandler::GetDirectShadowmap(i));
	}

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Skymap);

	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_3D, PreviousVolumeTextures[0]);

	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_3D, PreviousVolumeTextures[1]);

	glBindImageTexture(0, CurrentVolumeTextures[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);
	glBindImageTexture(1, CurrentVolumeTextures[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);

	glBindImageTexture(2, CurrentRawRadianceTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R11F_G11F_B10F);
	glBindImageTexture(3, PreviousRawRadianceTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R11F_G11F_B10F);

	glBindImageTexture(4, _PrevFrameDataTextures[0], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32UI);
	glBindImageTexture(5, _PrevFrameDataTextures[1], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32UI);
	//glBindImageTexture(8, VoxelVolume, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ProbeMapSSBO);

	Intersector.BindEverything(ProbeUpdate, true);

	glDispatchCompute(ProbeGridX / 8, ProbeGridY / 4, ProbeGridZ / 8);

	glUseProgram(0);

	// Copy data to buffer (feedback loop)

	CopyVolume.Use();

	CopyVolume.SetInteger("u_CurrentSHA", 0);
	CopyVolume.SetInteger("u_CurrentSHB", 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, CurrentVolumeTextures[0]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, CurrentVolumeTextures[1]);

	glBindImageTexture(2, _PrevFrameDataTextures[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);
	glBindImageTexture(3, _PrevFrameDataTextures[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);

	glDispatchCompute(ProbeGridX / 8, ProbeGridY / 4, ProbeGridZ / 8);

	glUseProgram(0);
}

void Candela::ProbeGI::UpdateProbes(int Frame, RayIntersector<BVH::StackTraversalNode>& Intersector, CommonUniforms& uniforms, GLuint Skymap, bool Temporal, int MaxFrames)
{
	const int ProbeGridX = PROBE_GRID_X;
	const int ProbeGridY = PROBE_GRID_Y;
	const int ProbeGridZ = PROBE_GRID_Z;

	GLClasses::ComputeShader& ProbeUpdate = ShaderManager::GetComputeShader("PROBE_UPDATE");
	GLClasses::ComputeShader& CopyVolume = ShaderManager::GetComputeShader("COPY_VOLUME");

	bool Checkerboard = Frame % 2 == 0;
	GLuint CurrentVolumeTextures[2] = { Checkerboard ? _ProbeDataTextures[0] : _PrevProbeDataTextures[0], Checkerboard ? _ProbeDataTextures[1] : _PrevProbeDataTextures[1] };
	GLuint PreviousVolumeTextures[2] = { (!Checkerboard) ? _ProbeDataTextures[0] : _PrevProbeDataTextures[0], (!Checkerboard) ? _ProbeDataTextures[1] : _PrevProbeDataTextures[1] };

	GLuint CurrentRawRadianceTexture = Checkerboard ? _ProbeRawRadianceBuffers[0] : _ProbeRawRadianceBuffers[1];
	GLuint PreviousRawRadianceTexture = Checkerboard ? _ProbeRawRadianceBuffers[1] : _ProbeRawRadianceBuffers[0];

	_CurrentDataTextures.x = CurrentVolumeTextures[0];
	_CurrentDataTextures.y = CurrentVolumeTextures[1];
	_CurrentDataTextures.z = CurrentRawRadianceTexture;

	ProbeUpdate.Use();

	_PreviousOrigin = LastOrigin;

	LastOrigin = glm::vec3(uniforms.InvView[3]);
	LastOrigin.x = Align(LastOrigin.x, 1.0f);
	LastOrigin.y = Align(LastOrigin.y, 1.0f);
	LastOrigin.z = Align(LastOrigin.z, 1.0f);

	ProbeUpdate.SetVector3f("u_SunDirection", uniforms.SunDirection);
	ProbeUpdate.SetVector3f("u_BoxOrigin", LastOrigin);
	ProbeUpdate.SetVector3f("u_Resolution", glm::vec3(ProbeGridX, ProbeGridY, ProbeGridZ));
	ProbeUpdate.SetVector3f("u_Size", PROBE_GRID_SIZE);
	ProbeUpdate.SetVector3f("u_PreviousOrigin", _PreviousOrigin);
	ProbeUpdate.SetFloat("u_Time", glfwGetTime());

	ProbeUpdate.SetInteger("u_Skymap", 4);

	ProbeUpdate.SetInteger("u_PreviousSHA", 5);
	ProbeUpdate.SetInteger("u_PreviousSHB", 6);
	ProbeUpdate.SetFloat("u_MaxFrames", MaxFrames);
	ProbeUpdate.SetBool("u_Temporal", Temporal);

	for (int i = 0; i < 5; i++) {

		const int BindingPointStart = 8;

		std::string Name = "u_ShadowMatrices[" + std::to_string(i) + "]";
		std::string NameClip = "u_ShadowClipPlanes[" + std::to_string(i) + "]";
		std::string NameTex = "u_ShadowTextures[" + std::to_string(i) + "]";

		ProbeUpdate.SetMatrix4(Name, ShadowHandler::GetShadowViewProjectionMatrix(i));
		ProbeUpdate.SetInteger(NameTex, i + BindingPointStart);
		ProbeUpdate.SetFloat(NameClip, ShadowHandler::GetShadowCascadeDistance(i));

		glActiveTexture(GL_TEXTURE0 + i + BindingPointStart);
		glBindTexture(GL_TEXTURE_2D, ShadowHandler::GetDirectShadowmap(i));
	}

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Skymap);

	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_3D, PreviousVolumeTextures[0]);

	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_3D, PreviousVolumeTextures[1]);

	glBindImageTexture(0, CurrentVolumeTextures[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);
	glBindImageTexture(1, CurrentVolumeTextures[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);

	glBindImageTexture(2, CurrentRawRadianceTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R11F_G11F_B10F);
	glBindImageTexture(3, PreviousRawRadianceTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R11F_G11F_B10F);

	glBindImageTexture(4, _PrevFrameDataTextures[0], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32UI);
	glBindImageTexture(5, _PrevFrameDataTextures[1], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32UI);
	//glBindImageTexture(8, VoxelVolume, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ProbeMapSSBO);

	Intersector.BindEverything(ProbeUpdate, true);

	glDispatchCompute(ProbeGridX / 8, ProbeGridY / 4, ProbeGridZ / 8);

	glUseProgram(0);

	// Copy data to buffer (feedback loop)

	CopyVolume.Use();

	CopyVolume.SetInteger("u_CurrentSHA", 0);
	CopyVolume.SetInteger("u_CurrentSHB", 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, CurrentVolumeTextures[0]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, CurrentVolumeTextures[1]);

	glBindImageTexture(2, _PrevFrameDataTextures[0], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);
	glBindImageTexture(3, _PrevFrameDataTextures[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32UI);

	glDispatchCompute(ProbeGridX / 8, ProbeGridY / 4, ProbeGridZ / 8);

	glUseProgram(0);
}

glm::vec3 Candela::ProbeGI::GetProbeBoxOrigin()
{
	return LastOrigin;
}

GLuint Candela::ProbeGI::GetProbeDataSSBO()
{
	return _ProbeMapSSBO;
}

glm::uvec2 Candela::ProbeGI::GetProbeDataTextures()
{
	return glm::uvec2(_CurrentDataTextures);
}

GLuint Candela::ProbeGI::GetProbeColorTexture()
{
	return _CurrentDataTextures.z;
}




//
//GLuint Candela::ProbeGI::GetVoxelVolume() {
//	return VoxelVolume;
//}

