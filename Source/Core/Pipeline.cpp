/*
The Candela Game Engine 
Written by : Samuel Rasquinha (samuelrasquinha@gmail.com)
			 @swr06 on github

Notes :

Pipeline : 
Pipeline.cpp contains the main pipeline code of the engine.
Head to "Candela::StartPipeline()" to change the demo scene/textures etc.

General :
Read controls.txt for controls 
"UNIFORM NOT FOUND" errors can safely be ignored.




PLEASE DO NOT CLAIM MY WORK AS YOUR OWN. 
*/

#define USE_STACKLESS_TRAVERSAL

#define STRINGIFY(x) (#x)


#include "Pipeline.h"

#include "FpsCamera.h"
#include "GLClasses/Shader.h"
#include "Object.h"
#include "Entity.h"
#include "ModelFileLoader.h" 
#include "ModelRenderer.h"
#include "GLClasses/Fps.h"
#include "GLClasses/Framebuffer.h"
#include "GLClasses/ComputeShader.h"
#include "ShaderManager.h"
#include "GLClasses/DepthBuffer.h"
#include "ShadowRenderer.h"
#include "GLClasses/CubeTextureMap.h"
#include "ProbeGI.h"
#include "Utils/Timer.h"

#include "SphereLight.h"

#include "Voxelizer.h"

#include "Tonemap.h"

#include "ProbeMap.h"

#include <string>

#include "ShadowMapHandler.h"

#include "BVH/BVHConstructor.h"
#include "BVH/Intersector.h"

#include "TAAJitter.h"

#include "Utility.h"

#include "Player.h"

#include "BloomRenderer.h"
#include "BloomFBO.h"

#include "Physics.h"

#include "PhysicsIntegrator.h"

#include "../Dependencies/imguizmo/ImGuizmo.h"

#include "LightCuller.h"

#include <implot.h>

// Externs.
int __TotalMeshesRendered = 0;
int __MainViewMeshesRendered = 0;

#ifdef USE_STACKLESS_TRAVERSAL
	Candela::RayIntersector<Candela::BVH::StacklessTraversalNode> Intersector;
#else
	Candela::RayIntersector<Candela::BVH::StackTraversalNode> Intersector;
#endif 

Candela::Player Player;
Candela::FPSCamera& Camera = Player.Camera;

static bool vsync = false;
static float SunTick = 50.0f;
static glm::vec3 _SunDirection = glm::vec3(0.1f, -1.0f, 0.1f);
static glm::vec3 PrevSunDir = _SunDirection;

static int ShadowmapRes = 1024;

// Options

// Refractions 

static bool RENDER_GLASS_FLAG = true;
static bool HQ_Refractions = false;
static bool OIT = false;
static bool StochasticTransparency = false;

// Misc 
static float InternalRenderResolution = 1.0f;
static float RoughnessMultiplier = 1.0f;
static bool GenerateHighFrequencyNormals = false;
static float NormalStrength = 0.4f;
static bool DoNormalFix = true;
static bool CollidePlayer = false;

// Perf
static bool DoFrustumCulling = false;
static bool DoFaceCulling = true;

// Direct shadow 
static float ShadowDistanceMultiplier = 1.0f;
static float ShadowNBiasMultiplier = 1.0f;
static float ShadowSBiasMultiplier = 1.0f;
static int ShadowmapUpdateRate = 1;

// GI
static bool DoMultiBounce = true;
static int SecondaryBounces = 1;
static bool DoInfiniteBounceGI = true;
static bool IndirectSSCaustics = true;
static bool DO_BL_SAMPLING = false;
static float RTAOStrength = 1.0f;

// Irradiance volume 
static bool UpdateIrradianceVolume = true;
static bool FilterIrradianceVolume = true;

// Specular 
static bool DoRoughSpecular = true;
static bool DoFullRTSpecular = false;

// Filtering 
static bool HQTextureFiltering = false;
static bool DoCheckering = true;

static bool DoTemporal = true;
static bool ClipDiffuse = false;
static float ClipStrength = 0.04f;

static float SpecularBeta = 4.5f;
static float SpecularTransversalWeight = 8.0f;

static bool DoSpatial = true;
static float SVGFStrictness = 0.2f;
static bool DoSVGF = true;
static bool DoSpatialUpscaling = true;

// Volumetrics 
static bool DoVolumetrics = true;
static float VolumetricsGlobalStrength = 1.0f;
static float VolumetricsDirectStrength = 1.0f;
static float VolumetricsIndirectStrength = 1.4f;
static int VolumetricsSteps = 16;
static bool VolumetricsTemporal = true;
static bool VolumetricsSpatial = true;
static bool UniformDensityFog = false;
static bool CompleteTransmittance = false;

// Screenspace shadow
static bool DoScreenspaceShadow = false;

// Voxelization
static bool DoVoxelization = false;

// Post 
static int SelectedTonemap = 0;

static bool DoTAA = true;
static float TAAStrengthMultiplier = 1.0f;
static float TAAClipBias = 0.0f;

static bool DoCAS = true;
static bool DoFXAA = true;
static float FXAAStrength = 0.15f;
static bool DoBloom = true;
static bool FSR = false;

// FSR 1.0
static float FSRUpscaleResolution = 1.0f;

// Exposure
static float ExposureMultiplier = 1.0f;

// DOF
static bool DoDOF = false;
static bool HQDOFBlur = false;
static bool PerformanceDOF = false;
static glm::vec2 DOFFocusPoint;
static float FocusDepthSmooth = 1.0f;
static float PlayerShadowSmooth = 1.0f;
static float DOFBlurRadius = 10.0f;
static float DOFScale = 0.01f;

// Purkinje 
static float PurkinjeStrength = 0.0f;

// Chromatic Aberration 
static float CAScale = 0.0f;

// Lens Flare
static float LensFlareStrength = 0.45f;

// Film Grain
static float GrainStrength = 0.f;

// Barrel/Pincushion Distortion
static bool DoDistortion = false;
static float DistortionK = 0.0f;

// Hemispherical Shadow Mapping 
// (Experiment)
const bool DoHSM = false;

// Player Probe
bool RENDER_PLAYER_PROBE = false;

// Timings
float CurrentTime = glfwGetTime();
float Frametime = 0.0f;
float DeltaTime = 0.0f;

// Debug views
static int SelectedDebugView = -1; 
static bool UpdateLightCullingVolume = false;

// Edit mode 
static bool EditMode = false;
static bool ShouldDrawGrid = true;
static int EditOperation = 0;

static Candela::Entity* SelectedEntity = nullptr;
static ImGuizmo::MODE Mode = ImGuizmo::MODE::LOCAL;
static bool UseSnap = true;
static glm::vec3 SnapSize = glm::vec3(0.5f);

// FBO debug 
static bool FBODebugMode = false;
static int FBODebugID = -1;
GLClasses::Framebuffer* DebugFBO = nullptr;
static int FBODebugAttachment = 0;

// Frame time plotter
static float* GraphX;
static float* GraphY;

// Render list 
std::vector<Candela::Entity*> EntityRenderList;

// GBuffers
GLClasses::Framebuffer GBuffers[2] = { GLClasses::Framebuffer(16, 16, {{GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, false, false}, {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, false, false}, {GL_R16I, GL_RED_INTEGER, GL_SHORT, false, false}}, false, true),GLClasses::Framebuffer(16, 16, {{GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, false, false}, {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, false, false}, {GL_R16I, GL_RED_INTEGER, GL_SHORT, false, false}}, false, true) };
GLClasses::Framebuffer TransparentGBuffer = GLClasses::Framebuffer(16, 16, { {GL_RGBA16F, GL_RGBA, GL_FLOAT, false, false}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, false, false} }, false, true);

// Skymap
GLClasses::CubeTextureMap Skymap;

// Draws editor grid 
void DrawGrid(const glm::mat4 CameraMatrix, const glm::mat4& ProjectionMatrix, const glm::vec3& GridBasis, float size) 
{
	glm::mat4 CurrentMatrix = glm::mat4(1.0f);
	//CurrentMatrix *= glm::translate(CurrentMatrix, glm::vec3(0.0f, -1.0f, 0.0f));
	//CurrentMatrix *= glm::rotate(CameraMatrix, glm::radians(90.0f), GridBasis);
	//CurrentMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(Camera.GetPosition().x, Camera.GetPosition().y - 20.0f, Camera.GetPosition().z));
	ImGuizmo::DrawGrid(glm::value_ptr(CameraMatrix), glm::value_ptr(ProjectionMatrix), value_ptr(CurrentMatrix), size);
}

static double RoundToNearest(double n, double x) {
	return round(n / x) * x;
}

void HandleFBODebug();

static void ClearAllBuffers();
bool WindowResizedThisFrame = false;

class RayTracerApp : public Candela::Application
{
public:

	RayTracerApp()
	{
		m_Width = 800;
		m_Height = 600;
	}

	void OnUserCreate(double ts) override
	{
	
	}

	void OnUserUpdate(double ts) override
	{
		glfwSwapInterval((int)vsync);

		GLFWwindow* window = GetWindow();
		float camera_speed = DeltaTime * 23.0f * ((glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS ? 3.0f : 1.0f));

		Player.OnUpdate(window, DeltaTime, camera_speed, GetCurrentFrame());
	}

	void OnImguiRender(double ts) override
	{
		ImGuiIO& io = ImGui::GetIO();

		if (EditMode) {

			if (ImGui::Begin("Debug/Edit Mode")) {

				if (!FBODebugMode) {
					// Drop down box
					const char* DebugLabelItems[] = { "Default", "Probe Debug", "Indirect Diffuse", "Ambient Occlusion", "Indirect Specular", "Direct Shadows", "Volumetrics", "Probe GI", "Albedo", "Normals", "Roughness", "Metalness", "Emissivity", "Voxelization"};
					static const char* CurrentDebugLabel = DebugLabelItems[0];

					if (ImGui::BeginCombo("##combo", CurrentDebugLabel))
					{
						for (int n = 0; n < IM_ARRAYSIZE(DebugLabelItems); n++)
						{
							bool is_selected = (CurrentDebugLabel == DebugLabelItems[n]);
							if (ImGui::Selectable(DebugLabelItems[n], is_selected))
							{
								SelectedDebugView = n - 1;
								CurrentDebugLabel = DebugLabelItems[n];
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					ImGui::NewLine();
					ImGui::NewLine();
				}

				ImGui::NewLine();
				ImGui::Checkbox("MANUAL FBO debug mode", &FBODebugMode);

				if (FBODebugMode) {
					ImGui::InputInt("ID", &FBODebugID);
					HandleFBODebug();
					if (DebugFBO) {
						ImGui::Text("Current Attachment : %d", FBODebugAttachment);
						ImGui::Text("Dimensions : %d x %d", DebugFBO->GetWidth(), DebugFBO->GetHeight());
						ImGui::Text("No of Attachments : %d", DebugFBO->m_TextureAttachments.size());
						ImGui::Text("FBO GL ID : %d", DebugFBO->GetFramebuffer());
					}

				}

				ImGui::NewLine();
				ImGui::NewLine();
				ImGui::NewLine();

				// Operations
				std::string OperationLabels[4] = { "Translation", "Rotation", "Scaling", "Universal (T/R/S)" };
				
				static bool space = 1;
				ImGui::Text("Editor Options");
				ImGui::Checkbox("Work in Local Space?", &space);
				ImGui::Checkbox("Draw Grid?", &ShouldDrawGrid);
				ImGui::Checkbox("Use Snap?", &UseSnap);
				if (UseSnap) {
					ImGui::SliderFloat3("Snap Size", &SnapSize[0], 0.01f, 4.0f);
				}
				ImGui::NewLine();
				ImGui::Text("Current Operation : %s", OperationLabels[EditOperation].c_str());
				ImGui::Text("Note : Use keys F6 - F9 to change operation (T/R/S/Universal)");

				Mode = space ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD;

				ImGui::NewLine();
				ImGui::NewLine();

			} ImGui::End();

			// Draw editor

			if (!FBODebugMode) {
				ImGuizmo::BeginFrame();

				ImGuizmo::SetOrthographic(false);
				ImGuizmo::SetRect(0, 0, GetWidth(), GetHeight());

				if (ShouldDrawGrid) {
					DrawGrid(Camera.GetViewMatrix(), Camera.GetProjectionMatrix(), glm::vec3(0.0f, 1.0f, 0.0f), 200.0f);
				}

				if (SelectedEntity) {

					const ImGuizmo::OPERATION Ops[4] = { ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::OPERATION::ROTATE, ImGuizmo::OPERATION::SCALE, ImGuizmo::OPERATION::UNIVERSAL };

					glm::mat4 ModelMatrix = glm::mat4(1.0f);

					glm::vec3 Offset;

					{
						glm::vec3 tMin = SelectedEntity->m_Object->Min;
						glm::vec3 tMax = SelectedEntity->m_Object->Max;

						tMin = glm::vec3(ModelMatrix * glm::vec4(tMin, 1.0f));
						tMax = glm::vec3(ModelMatrix * glm::vec4(tMax, 1.0f));

						Offset = (tMin + tMax) * 0.5f;
					}

					ModelMatrix = glm::translate(SelectedEntity->m_Model, Offset);

					ImGuizmo::Manipulate(glm::value_ptr(Camera.GetViewMatrix()), glm::value_ptr(Camera.GetProjectionMatrix()),
						Ops[glm::clamp(EditOperation, 0, 3)], Mode, glm::value_ptr(ModelMatrix), nullptr, UseSnap ? glm::value_ptr(SnapSize) : nullptr);

					SelectedEntity->m_Model = glm::translate(ModelMatrix, -Offset);

					bool Hovered = ImGuizmo::IsOver();
					bool Using = ImGuizmo::IsUsing();

					if (Hovered || Using) {
						glm::vec3 P, S, R;
						ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(SelectedEntity->m_Model), glm::value_ptr(P), glm::value_ptr(R), glm::value_ptr(S));

						if (SelectedEntity->m_IsPhysicsObject) {

							//SelectedEntity->m_PhysicsObject.Position = P;

						}
					}

					// Entity window 
					ImGui::Begin("Entity Settings");
					ImGui::NewLine();

					std::string filename = SelectedEntity->m_Object->Path;
					size_t Idx = filename.find_last_of("\\/");
					if (std::string::npos != Idx)
					{
						filename.erase(0, Idx + 1);
					}


					ImGui::Text("Entity has parent object with model : %s", filename.c_str());
					ImGui::Text("Entity Position : %f %f %f", SelectedEntity->m_Model[3].x, SelectedEntity->m_Model[3].y, SelectedEntity->m_Model[3].z);
					ImGui::SliderFloat("Emissivity", &SelectedEntity->m_EmissiveAmount, 0.0f, 32.0f);
					ImGui::SliderFloat("Entity Roughness Multiplier", &SelectedEntity->m_EntityRoughnessMultiplier, 0.0f, 8.0f);
					ImGui::SliderFloat("Entity Roughness", &SelectedEntity->m_EntityRoughness, 0.0f, 8.0f);
					ImGui::SliderFloat("Entity Metalness", &SelectedEntity->m_EntityMetalness, 0.0f, 1.0f);
					ImGui::SliderFloat("Translucency Amount", &SelectedEntity->m_TranslucencyAmount, 0.0f, 1.0f);
					ImGui::NewLine();
					ImGui::Checkbox("Use Albedo map?", &SelectedEntity->m_UseAlbedoMap);
					ImGui::Checkbox("Use PBR/Normal map?", &SelectedEntity->m_UsePBRMap);
					ImGui::End();
				}
			}

			// Draw frametime graph 
			ImGui::Begin("Frametime Graph");
			ImGui::NewLine();
			ImGui::Text("Frame Delta is : %f ms", DeltaTime * 1000.0f);
			ImGui::NewLine();
			if (ImPlot::BeginPlot("Frametime Plot (dt-time graph)")) {
				ImPlot::SetupAxisLimits(ImAxis_X1, this->GetTime() - 12.0, this->GetTime() + 1.0f, ImGuiCond_Always);
				ImPlot::PlotLine("Frametime Plot", GraphX, GraphY, 1024);
				ImPlot::EndPlot();
			}
			ImGui::End();
		}

		if (ImGui::Begin("Options")) {
			ImGui::Text("Camera Position : %f,  %f,  %f", Camera.GetPosition().x, Camera.GetPosition().y, Camera.GetPosition().z);
			ImGui::Text("Camera Front : %f,  %f,  %f", Camera.GetFront().x, Camera.GetFront().y, Camera.GetFront().z);
			ImGui::Text("Time : %f s", glfwGetTime());
			ImGui::NewLine();
			ImGui::Text("Number of Meshes Rendered (For the main camera view) : %d", __MainViewMeshesRendered);
			ImGui::Text("Total Number of Meshes Rendered : %d", __TotalMeshesRendered);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::SliderFloat3("Sun Direction", &_SunDirection[0], -1.0f, 1.0f);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Text("-- Rendering --");
			ImGui::NewLine();
			ImGui::SliderFloat("Internal Render Resolution", &InternalRenderResolution, 0.25f, 2.0f);
			ImGui::NewLine();
			ImGui::SliderFloat("Roughness Multiplier", &RoughnessMultiplier, 0.0f, 3.0f);
			ImGui::Checkbox("High Quality Texture Filtering?", &HQTextureFiltering);
			ImGui::Checkbox("Generate High Frequency Normals?", &GenerateHighFrequencyNormals);

			if (GenerateHighFrequencyNormals)
				ImGui::SliderFloat("HF Normal Strength", &NormalStrength, 0.0f, 1.0f);

			ImGui::NewLine();
			ImGui::Checkbox("Do Normal Fix? (Artifacts possible; Normal flips when view angle is > 180°)", &DoNormalFix);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Translucent Rendering?", &RENDER_GLASS_FLAG);
			ImGui::Checkbox("Use OIT? (if off, uses ssrt refractions)", &OIT);

			if (OIT)
				ImGui::Checkbox("Use Stochastic Transparency", &StochasticTransparency);

			if (!OIT)
				ImGui::Checkbox("HQ Refractions?", &HQ_Refractions);

			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Frustum Culling?", &DoFrustumCulling);
			ImGui::Checkbox("Face Culling?", &DoFaceCulling);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::SliderFloat("Shadow Distance Multiplier", &ShadowDistanceMultiplier, 0.1f, 16.0f);
			ImGui::SliderInt("Shadowmap Resolution", &ShadowmapRes, 256, 8096);
			ImGui::SliderInt("Shadowmap Update Rate (Increasing this parameter will result in more responsive shadows at the cost of performance)", &ShadowmapUpdateRate, 1, 5);
			ImGui::SliderFloat("Shadow Normal Bias Multiplier (To reduce shadow acne/flickering)", &ShadowNBiasMultiplier, 0.25f, 6.0f);
			ImGui::SliderFloat("Shadow Sample Bias Multiplier (To reduce shadow acne/flickering)", &ShadowSBiasMultiplier, 0.25f, 8.0f);
			ImGui::NewLine();
			ImGui::Checkbox("Do Screenspace Shadows? (More detailed small scale shadows.)", &DoScreenspaceShadow);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Update Irradiance Volume?", &UpdateIrradianceVolume);
			ImGui::Checkbox("Temporally Filter Irradiance Volume?", &FilterIrradianceVolume);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Do Diffuse Multi Bounce?", &DoMultiBounce);

			if (DoMultiBounce) {
				ImGui::Checkbox("Infinite Bounce GI?", &DoInfiniteBounceGI);

				if (!DoInfiniteBounceGI) {
					ImGui::SliderInt("Secondary Bounces", &SecondaryBounces, 1, 8);
				}
			}

			if (RENDER_GLASS_FLAG) {
				//ImGui::Checkbox("Screenspace Indirect Caustics?", &IndirectSSCaustics);
			}

			ImGui::SliderFloat("Ray Traced Ambient Occlusion Strength", &RTAOStrength, 0.0f, 4.0f);

			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Rough Specular?", &DoRoughSpecular);
			ImGui::Checkbox("Full Worldspace RT Specular GI?", &DoFullRTSpecular);

			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Volumetrics?", &DoVolumetrics);

			if (DoVolumetrics) {
				ImGui::SliderFloat("Volumetrics Strength", &VolumetricsGlobalStrength, 0.1f, 6.0f);
				ImGui::SliderFloat("Volumetrics Direct Strength", &VolumetricsDirectStrength, 0.0f, 8.0f);
				ImGui::SliderFloat("Volumetrics Indirect Strength", &VolumetricsIndirectStrength, 0.0f, 8.0f);
				ImGui::SliderInt("Volumetrics Steps", &VolumetricsSteps, 4, 128);

				if (DoTemporal) {
					ImGui::Checkbox("Temporally Filter Volumetrics? (Cleaner, more temporal lag)", &VolumetricsTemporal);
				}

				if (DoSpatial) {
					ImGui::Checkbox("Spatially Filter Volumetrics?", &VolumetricsSpatial);
				}

				ImGui::Checkbox("Complete VL Transmittance?", &CompleteTransmittance);
				ImGui::Checkbox("Uniform Density Volumetric Fog?", &UniformDensityFog);
			}

			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Do Voxelization?", &DoVoxelization);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Update Light Culling Volume?", & UpdateLightCullingVolume);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("Checkerboard Lighting? (effectively computes lighting for half the pixels)", &DoCheckering);
			ImGui::NewLine();
			ImGui::Checkbox("Use Blue Noise Sampling?", &DO_BL_SAMPLING);
			ImGui::NewLine();
			ImGui::Checkbox("Temporal Filtering?", &DoTemporal);

			if (DoTemporal)
				ImGui::Checkbox("Enable Temporal Clipping? (Reduces temporal lag substantially but increases noise slightly)", &ClipDiffuse);
			if (ClipDiffuse)
				ImGui::SliderFloat("Clip Strength", &ClipStrength, 0.005f, 0.08f);

			ImGui::NewLine();
			ImGui::Checkbox("Spatial Filtering?", &DoSpatial);
			if (DoSpatial) {
				ImGui::Checkbox("Use Variance Guided Filter?", &DoSVGF);
				if (DoSVGF)
					ImGui::SliderFloat("SVGF Strictness", &SVGFStrictness, 0.0f, 5.0f);

				ImGui::SliderFloat("Specular Lobe Weight Beta (Higher = More detail, more noise.)", &SpecularBeta, 0.1f, 32.0f);
				ImGui::SliderFloat("Specular Transversal Weight (Higher = Better Contact Hardening, more noise.)", &SpecularTransversalWeight, 0.2f, 32.0f);
			}
			
			ImGui::NewLine();
			ImGui::Checkbox("Spatial Upscaling?", &DoSpatialUpscaling);
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::NewLine();

			ImGui::Text("-- Post Process --");
			ImGui::NewLine();
			ImGui::Checkbox("Temporal Anti Aliasing?", &DoTAA);
			
			if (DoTAA) {
				ImGui::SliderFloat("TAA Strength Multiplier", &TAAStrengthMultiplier, 0.0f, 2.0f);
				ImGui::SliderFloat("TAA Clip Bias (Higher values = More ghosting and less flickering)", &TAAClipBias, 0.0f, 0.15f);
			}

			if (InternalRenderResolution < 0.99f && DoTAA) {
				ImGui::Checkbox("AMD FSR?", &FSR);
			}

			ImGui::NewLine();

			ImGui::Checkbox("Contrast Adaptive Sharpening?", &DoCAS);
			ImGui::NewLine();
			ImGui::Checkbox("Fast Approximate Anti Aliasing?", &DoFXAA);

			if (DoFXAA)
				ImGui::SliderFloat("FXAA Strength", &FXAAStrength, 0.001f, 0.999f);

			ImGui::NewLine();
			ImGui::NewLine();
			
			// Tonemap types 
			const char* PostProcessTonemaps[] = { "ACES", "TonyMcMapFace", "ACES Filmic/Cinematic", "Lottes", "Hejl Burgess", "RomBinDaHouse", "Uncharted 2", "None"};
			static const char* CurrentTonemapLabel = PostProcessTonemaps[0];

			if (ImGui::BeginCombo("Tonemappers", CurrentTonemapLabel))
			{
				for (int n = 0; n < IM_ARRAYSIZE(PostProcessTonemaps); n++)
				{
					bool is_selected = (CurrentTonemapLabel == PostProcessTonemaps[n]);
					if (ImGui::Selectable(PostProcessTonemaps[n], is_selected))
					{
						SelectedTonemap = n;
						CurrentTonemapLabel = PostProcessTonemaps[n];
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}


			
			ImGui::SliderFloat("Camera Exposure Multiplier", &ExposureMultiplier, 0.001f, 8.0f);
			ImGui::NewLine();
			ImGui::Checkbox("Bloom?", &DoBloom);
			ImGui::Checkbox("Depth of Field?", &DoDOF);
			if (DoDOF) {
				ImGui::SliderFloat("DOF Blur Radius", &DOFBlurRadius, 2.0f, 16.0f);
				ImGui::SliderFloat("DOF Blur Scale", &DOFScale, 0.001f, 0.05f);
				ImGui::Checkbox("Performance DOF?", &PerformanceDOF);
				ImGui::Checkbox("High Quality DOF Bokeh?", &HQDOFBlur);
			}

			ImGui::NewLine();
			ImGui::SliderFloat("Chromatic Aberration Strength", &CAScale, 0.0f, 0.50f);
			ImGui::SliderFloat("Film Grain Strength", &GrainStrength, 0.0f, 1.0f);
			ImGui::SliderFloat("Lens Flare Strength", &LensFlareStrength, 0.0f, 6.0f);
			ImGui::NewLine();
			ImGui::SliderFloat("Purkinje Effect Intensity", &PurkinjeStrength, 0.0f, 3.0f);
			ImGui::NewLine();
			ImGui::Checkbox("Distortion? (Barrel/Pincushion)", &DoDistortion);
			
			if (DoDistortion)
				ImGui::SliderFloat("Distortion Coefficient (Pincushion if -ve and Barrel if +ve)", &DistortionK, -1.0f, 1.0f);
			
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::NewLine();
			ImGui::Checkbox("WIP : Test For Collisions (refer console for output)", &CollidePlayer);

		} ImGui::End();

		__TotalMeshesRendered = 0;
		__MainViewMeshesRendered = 0;
	}

	void OnEvent(Candela::Event e) override
	{
		ImGuiIO& io = ImGui::GetIO();
		
		if (e.type == Candela::EventTypes::MousePress && EditMode && !ImGui::GetIO().WantCaptureMouse)
		{
			if (!this->GetCursorLocked()) {

				if (!ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
					double mxx, myy;
					glfwGetCursorPos(this->m_Window, &mxx, &myy);
					mxx *= InternalRenderResolution;
					myy = (double)(this->GetHeight() - myy) * InternalRenderResolution;

					float d1, d2;

					glBindFramebuffer(GL_FRAMEBUFFER, GBuffers[0].GetFramebuffer());
					glReadPixels((int)mxx, (int)myy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d1);

					glBindFramebuffer(GL_FRAMEBUFFER, TransparentGBuffer.GetFramebuffer());
					glReadPixels((int)mxx, (int)myy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d2);

					if (d1 < d2) {
						short read_data = 0;
						glBindFramebuffer(GL_FRAMEBUFFER, GBuffers[0].GetFramebuffer());
						glReadBuffer(GL_COLOR_ATTACHMENT0 + 4);
						glReadPixels((int)mxx, (int)myy, 1, 1, GL_RED_INTEGER, GL_SHORT, &read_data);

						if (read_data >= 2) {
							read_data -= 2;
							SelectedEntity = EntityRenderList[read_data];
						}
					}

					else {
						glm::vec4 read_data;
						glBindFramebuffer(GL_FRAMEBUFFER, TransparentGBuffer.GetFramebuffer());
						glReadBuffer(GL_COLOR_ATTACHMENT0 + 0);
						glReadPixels((int)mxx, (int)myy, 1, 1, GL_RGBA, GL_FLOAT, &read_data);

						int data = int(floor(read_data.w));

						if (data >= 2) {
							data -= 2;
							SelectedEntity = EntityRenderList[data];
						}
					}
				}
			}
		}

		if (e.type == Candela::EventTypes::MouseMove && GetCursorLocked())
		{
			Camera.UpdateOnMouseMovement(e.mx, e.my);
		}


		if (e.type == Candela::EventTypes::MouseScroll && !ImGui::GetIO().WantCaptureMouse)
		{
			float Sign = e.msy < 0.0f ? 1.0f : -1.0f;
			Camera.SetFov(Camera.GetFov() + 2.0f * Sign);
			Camera.SetFov(glm::clamp(Camera.GetFov(), 1.0f, 89.0f));
		}

		if (e.type == Candela::EventTypes::WindowResize)
		{
			Camera.SetAspect((float)glm::max(e.wx, 1) / (float)glm::max(e.wy, 1));
			WindowResizedThisFrame = true && this->GetCurrentFrame() > 4;
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_ESCAPE) {
			exit(0);
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_F1)
		{
			this->SetCursorLocked(!this->GetCursorLocked());
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_F2 && this->GetCurrentFrame() > 5)
		{
			Candela::ShaderManager::RecompileShaders();
			Candela::Voxelizer::RecompileShaders();
			Candela::LightCuller::RecompileShaders();
			Intersector.Recompile();
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_F3 && this->GetCurrentFrame() > 5)
		{
			Candela::ShaderManager::ForceRecompileShaders();
			Intersector.Recompile();
		}
		
		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_F5 && this->GetCurrentFrame() > 5)
		{
			EditMode = !EditMode;
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_F6 && this->GetCurrentFrame() > 5) {
			EditOperation = 0;
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_F7 && this->GetCurrentFrame() > 5) {
			EditOperation = 1;
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_F8 && this->GetCurrentFrame() > 5) {
			EditOperation = 2;
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_F9 && this->GetCurrentFrame() > 5) {
			EditOperation = 3;
		}

		if (e.type == Candela::EventTypes::KeyPress && e.key == GLFW_KEY_V && this->GetCurrentFrame() > 5)
		{
			vsync = !vsync;
		}

		if (e.type == Candela::EventTypes::MousePress && !ImGui::GetIO().WantCaptureMouse)
		{
			if (!this->GetCursorLocked()) {
				double mxx, myy;
				glfwGetCursorPos(this->m_Window, &mxx, &myy);
				myy = (double)this->GetHeight() - myy;
				DOFFocusPoint = glm::vec2((float)mxx, (float)myy);
			} 
		}
	}


};


void RenderEntityList(const std::vector<Candela::Entity*> EntityList, GLClasses::Shader& shader, bool glasspass) {

	int En = 0;
		
	for (auto& e : EntityList) {
		Candela::RenderEntity(*e, shader, Player.CameraFrustum, DoFrustumCulling, En, glasspass);
		En++;
	}
}


void UnbindEverything() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
}


void RenderProbe(Candela::ProbeMap& probe, int face, glm::vec3 center, std::vector<Candela::Entity*> EntityList, GLClasses::Shader& shader) {

	if (face >= 6) {
		throw "What.";
	}

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	probe.CapturePoints[face] = center;

	const glm::mat4 projection_matrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 800.0f);
	const glm::mat4 inverse_projection = glm::inverse(projection_matrix);

	std::array<glm::mat4, 6> view_matrices =
	{
		glm::lookAt(center, center + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(center, center + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(center, center + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(center, center + glm::vec3(0.0f,-1.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(center, center + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(center, center + glm::vec3(0.0f, 0.0f,-1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	probe.BindFace(face, false);
	shader.Use();   
	glClearColor(0., 0., 0., 0.);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	shader.SetVector3f("u_CapturePosition", center);
	shader.SetMatrix4("u_ViewProjection", projection_matrix * view_matrices[face]);
	shader.SetInteger("u_AlbedoMap", 0);
	shader.SetInteger("u_NormalMap", 1);
	shader.SetInteger("u_RoughnessMap", 2);
	shader.SetInteger("u_MetalnessMap", 3);
	shader.SetInteger("u_MetalnessRoughnessMap", 5);
	shader.SetBool("u_NormalFix", DoNormalFix);

	RenderEntityList(EntityList, shader, false);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	//glDisable(GL_CULL_FACE);
}


template <typename T>
void SetCommonUniforms(T& shader, CommonUniforms& uniforms) {
	shader.SetFloat("u_Time", glfwGetTime());
	shader.SetInteger("u_Frame", uniforms.Frame);
	shader.SetInteger("u_CurrentFrame", uniforms.Frame);
	shader.SetMatrix4("u_ViewProjection", Camera.GetViewProjection());
	shader.SetMatrix4("u_Projection", uniforms.Projection);
	shader.SetMatrix4("u_View", uniforms.View);
	shader.SetMatrix4("u_InverseProjection", uniforms.InvProjection);
	shader.SetMatrix4("u_InverseView", uniforms.InvView);
	shader.SetMatrix4("u_PrevProjection", uniforms.PrevProj);
	shader.SetMatrix4("u_PrevView", uniforms.PrevView);
	shader.SetMatrix4("u_PrevInverseProjection", uniforms.InvPrevProj);
	shader.SetMatrix4("u_PrevInverseView", uniforms.InvPrevView);
	shader.SetMatrix4("u_InversePrevProjection", uniforms.InvPrevProj);
	shader.SetMatrix4("u_InversePrevView", uniforms.InvPrevView);
	shader.SetVector3f("u_ViewerPosition", glm::vec3(uniforms.InvView[3]));
	shader.SetVector3f("u_Incident", glm::vec3(uniforms.InvView[3]));
	shader.SetVector3f("u_SunDirection", uniforms.SunDirection);
	shader.SetVector3f("u_LightDirection", uniforms.SunDirection);
	shader.SetFloat("u_zNear", Camera.GetNearPlane());
	shader.SetFloat("u_zFar", Camera.GetFarPlane());
}

// Deferred
GLClasses::Framebuffer LightingPass(16, 16, { {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true} }, false, false);

// Transparency passes
GLClasses::Framebuffer TempFramebuffer(16, 16, { {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true} }, false, false);

// Forward pass
GLClasses::Framebuffer TransparentPass(16, 16, { {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, {GL_R16F, GL_RED, GL_FLOAT, true, true} }, false, false);

// Post 
GLClasses::Framebuffer Composited(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false);
GLClasses::Framebuffer VolComposited(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false);
GLClasses::Framebuffer PFXComposited(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false);
GLClasses::Framebuffer DOF(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false);
GLClasses::Framebuffer VolumetricsCheckerboardBuffers[2]{ GLClasses::Framebuffer(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false), GLClasses::Framebuffer(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false) };

// For temporal 
GLClasses::Framebuffer MotionVectors(16, 16, { GL_RG16F, GL_RG, GL_FLOAT, true, true }, false, false);

// Raw output 
GLClasses::Framebuffer DiffuseCheckerboardBuffers[2]{ GLClasses::Framebuffer(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false), GLClasses::Framebuffer(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false) };
GLClasses::Framebuffer SpecularCheckerboardBuffers[2]{ GLClasses::Framebuffer(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false), GLClasses::Framebuffer(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false) };
GLClasses::Framebuffer SSRefractions(16, 16, { GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true }, false, false);

// Upscaling 
GLClasses::Framebuffer CheckerboardUpscaled(16, 16, { { GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true },{ GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true }, { GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true } }, false, false);
GLClasses::Framebuffer TemporalBuffersIndirect[2]{ GLClasses::Framebuffer(16, 16, { {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, {GL_RG16F, GL_RED, GL_FLOAT, true, true}, {GL_RG16F, GL_RG, GL_FLOAT, true, true}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true},  {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}}, false, false), GLClasses::Framebuffer(16, 16, { {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, {GL_RG16F, GL_RED, GL_FLOAT, true, true}, {GL_RG16F, GL_RG, GL_FLOAT, true, true}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true},  {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true} }, false, false) };

// Denoiser 
GLClasses::Framebuffer SpatialVariance(16, 16, { { GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true }, { GL_R16F, GL_RED, GL_FLOAT, true, true } }, false, false);
GLClasses::Framebuffer SpatialUpscaled(16, 16, { { GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true }, { GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true }, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true} }, false, false);
GLClasses::Framebuffer SpatialBuffers[2]{ GLClasses::Framebuffer(16, 16, {{GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, {GL_R16F, GL_RED, GL_FLOAT, true, true}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}}, false, false),GLClasses::Framebuffer(16, 16, {{GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, {GL_R16F, GL_RED, GL_FLOAT, true, true}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}}, false, false) };

// Antialiasing 
GLClasses::Framebuffer TAABuffers[2] = { GLClasses::Framebuffer(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false), GLClasses::Framebuffer(16, 16, {GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true}, false, false) };

static std::vector <GLClasses::Framebuffer*> FramebufferList {
		&LightingPass, &TempFramebuffer,
		&TransparentPass, &Composited,
		&VolComposited, &PFXComposited,
		&DOF, &VolumetricsCheckerboardBuffers[0],
		&VolumetricsCheckerboardBuffers[1], &MotionVectors,
		&DiffuseCheckerboardBuffers[0], &DiffuseCheckerboardBuffers[1],
		&SpecularCheckerboardBuffers[0], &SpecularCheckerboardBuffers[1],
		&SSRefractions, &CheckerboardUpscaled,
		&TemporalBuffersIndirect[0], &TemporalBuffersIndirect[1],
		&SpatialVariance, &SpatialUpscaled,
		&SpatialBuffers[0], &SpatialBuffers[1],
		&TAABuffers[0], &TAABuffers[1]
	};

static void ClearAllBuffers() {

	for (auto& e : FramebufferList) {
		if (e != nullptr) {
			if (e->GetFramebuffer() != 0) {
				e->Bind();
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			}
		}
	}
}

static void HandleFBODebug() {
	if (!FBODebugMode) {
		return;
	}

	int x = 0;

	DebugFBO = nullptr;

	for (int i = 0; i < FramebufferList.size(); i++) {
		for (int j = 0; j < FramebufferList[i]->m_TextureAttachments.size(); j++) {
			if (x == FBODebugID) {
				DebugFBO = FramebufferList[i];
				FBODebugAttachment = j;
			}
			x++;
		}
	}
}

// Entry point 
void Candela::StartPipeline()
{
	//////////////////////////////////////
	////// -- Demo Program Code -- ///////
	//////////////////////////////////////
	 
	// Matrices to rotate models whose axis basis is different 
	const glm::mat4 ZOrientMatrix = glm::mat4(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), glm::vec4(1.0f));
	const glm::mat4 ZOrientMatrixNegative = glm::mat4(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, -1.0f, 0.0f, 0.0f), glm::vec4(1.0f));

	using namespace BVH;

	GraphX = new float[1024];
	GraphY = new float[1024];

	memset(GraphX, 0, 1024 * sizeof(float));
	memset(GraphY, 0, 1024 * sizeof(float));

	// Create App, initialize 
	RayTracerApp app;
	app.Initialize();
	app.SetCursorLocked(true);
	ImPlot::CreateContext();

	// Scene setup 
	Object MainModel;
	Object Dragon;
	Object MetalObject;
	Object Sphere;
	
	// Load demo models 
	FileLoader::LoadModelFile(&MetalObject, "Models/ball/scene.gltf");
	FileLoader::LoadModelFile(&MainModel, "Models/sponza-2/sponza.obj");
	FileLoader::LoadModelFile(&Dragon, "Models/dragon/dragon.obj");
	FileLoader::LoadModelFile(&Sphere, "Models/sphere/scene.gltf");

	// - Test models -
	// uncomment to try them out :)
	//FileLoader::LoadModelFile(&MainModel, "Models/architecture/scene.gltf");
	//FileLoader::LoadModelFile(&MainModel, "Models/living_room/living_room.obj");
	//FileLoader::LoadModelFile(&MainModel, "Models/sponza-pbr/sponza.gltf");
	//FileLoader::LoadModelFile(&MetalObject, "Models/monke/Suzanne.gltf");
	//FileLoader::LoadModelFile(&MainModel, "Models/gitest/multibounce_gi_test_scene.gltf");
	//FileLoader::LoadModelFile(&MainModel, "Models/sonic/N64 Yoshi Valley.obj");
	//FileLoader::LoadModelFile(&MainModel, "Models/zeldamk/Wii Wario's Gold Mine.obj");
	//FileLoader::LoadModelFile(&MainModel, "Models/thwomp/Thwomp Ruins.obj");
	//FileLoader::LoadModelFile(&MainModel, "Models/csgo/scene.gltf");
	//FileLoader::LoadModelFile(&MainModel, "Models/fireplace_room/fireplace_room.obj");
	//FileLoader::LoadModelFile(&MainModel, "Models/mc/scene.gltf");
	//FileLoader::LoadModelFile(&MainModel, "Models/peachcastle/Castle.obj");
	
	// Add objects to intersector
	Intersector.Initialize();

	// Add the objects to the intersector (you could use a vector or similar to make this generic)
	Intersector.AddObject(MainModel);
	Intersector.AddObject(Dragon);
	Intersector.AddObject(MetalObject);
	Intersector.AddObject(Sphere);

	Intersector.BufferData(true); // The flag is to tell the intersector to delete the cached cpu data 
	Intersector.GenerateMeshTextureReferences(); // This function is called to generate the texture references for the BVH

	// Create entities, each entity has a parent object 
	// Entities can have an arbitrary model matrix, transparency etc.

	// Create the main model 
	Entity MainModelEntity(&MainModel);
	MainModelEntity.m_EntityRoughness = 0.65f;
	//MainModelEntity.m_Model = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
	//MainModelEntity.m_Model = glm::scale(glm::mat4(1.0f), glm::vec3(10.0f));
	//MainModelEntity.m_Model *= ZOrientMatrixNegative;
	//MainModelEntity.m_Model *= ZOrientMatrix;

	// Create the opaque dragon 
	Entity DragonEntity(&Dragon); 
	DragonEntity.m_EmissiveAmount = 15.0f;
	DragonEntity.m_Model = glm::translate(glm::mat4(1.), glm::vec3(-0.7f, 0.5f, -4.5f));
	DragonEntity.m_Model *= glm::scale(glm::mat4(1.), glm::vec3(0.14f));

	// Create the glass dragon
	Entity GlassDragon(&Dragon);
	GlassDragon.m_EmissiveAmount = 0.0f;
	GlassDragon.m_Model = glm::translate(glm::mat4(1.), glm::vec3(-4.25f, 0.5f, -0.5f));
	GlassDragon.m_Model *= glm::scale(glm::mat4(1.), glm::vec3(0.14f));
	GlassDragon.m_TranslucencyAmount = 0.4f;

	// Create metal ball 
	Entity MetalObjectEntity(&MetalObject);
	glm::vec3 MetalObjectStartPosition = glm::vec3(-1.0f, 1.25f, -2.0f);
	MetalObjectEntity.m_Model = glm::translate(glm::mat4(1.0f), MetalObjectStartPosition);

	// Create sphere 
	Entity SphereEntity(&Sphere);
	SphereEntity.m_Model = glm::translate(glm::mat4(1.0f), glm::vec3(-14.0f, 6.25f, -0.1f));
	SphereEntity.m_IsSphereLight = true; 
	SphereEntity.m_EmissiveAmount = 8.0f;

	// Add entities to the render list 
	EntityRenderList = { &MainModelEntity, &DragonEntity, &MetalObjectEntity, &GlassDragon, &SphereEntity };

	// Create the environment map (the environment map is arbitrary) 

	Skymap.CreateCubeTextureMap(
		{
		"Res/Skymap/right.bmp",
		"Res/Skymap/left.bmp",
		"Res/Skymap/top.bmp",
		"Res/Skymap/bottom.bmp",
		"Res/Skymap/front.bmp",
		"Res/Skymap/back.bmp"
		}, true
	);



	/////////////////////////////////////////
	/////////////////////////////////////////
	/////////////////////////////////////////
	////// -- Internal engine code -- ///////
	/////////////////////////////////////////
	/////////////////////////////////////////
	/////////////////////////////////////////

	// Create VBO and VAO for drawing the screen-sized quad.
	GLClasses::VertexBuffer ScreenQuadVBO;
	GLClasses::VertexArray ScreenQuadVAO;

	// Setup screensized quad for rendering
	{
		unsigned long long CurrentFrame = 0;
		float QuadVertices_NDC[] =
		{
			-1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f, -1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f
		};

		ScreenQuadVAO.Bind();
		ScreenQuadVBO.Bind();
		ScreenQuadVBO.BufferData(sizeof(QuadVertices_NDC), QuadVertices_NDC, GL_STATIC_DRAW);
		ScreenQuadVBO.VertexAttribPointer(0, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), 0);
		ScreenQuadVBO.VertexAttribPointer(1, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
		ScreenQuadVAO.Unbind();
	}

	GLClasses::Texture BlueNoise;
	BlueNoise.CreateTexture("Res/blue_noise.png", false, false);
	GLClasses::Texture BlueNoiseHR;
	BlueNoiseHR.CreateTexture("Res/bluenoise_hr.png", false, false);

	// Create Shaders
	ShaderManager::CreateShaders();

	GLClasses::Shader& GBufferShader = ShaderManager::GetShader("GBUFFER");
	GLClasses::Shader& GBufferTransparentPrepassShader = ShaderManager::GetShader("GLASS_PREPASS");

	GLClasses::Shader& TransparentForwardShader = ShaderManager::GetShader("TRANSPARENT_FORWARD");
	GLClasses::Shader& OITCompositeShader = ShaderManager::GetShader("OIT_COMPOSITE");

	GLClasses::Shader& LightingShader = ShaderManager::GetShader("LIGHTING_PASS");

	GLClasses::Shader& CheckerReconstructShader = ShaderManager::GetShader("CHECKER_UPSCALE");
	GLClasses::Shader& TemporalFilterShader = ShaderManager::GetShader("TEMPORAL");
	GLClasses::Shader& MotionVectorShader = ShaderManager::GetShader("MOTION_VECTORS");

	GLClasses::Shader& SpatialVarianceShader = ShaderManager::GetShader("SVGF_VARIANCE");
	GLClasses::Shader& SpatialFilterShader = ShaderManager::GetShader("SPATIAL_FILTER");
	GLClasses::Shader& SpatialUpscaleShader = ShaderManager::GetShader("SPATIAL_UPSCALE");

	GLClasses::Shader& TAAShader = ShaderManager::GetShader("TAA");
	GLClasses::Shader& CASShader = ShaderManager::GetShader("CAS");

	GLClasses::Shader& SSRefractionShader = ShaderManager::GetShader("SS_REFRACT");

	GLClasses::ComputeShader& DiffuseShader = ShaderManager::GetComputeShader("DIFFUSE_TRACE");
	GLClasses::ComputeShader& SpecularShader = ShaderManager::GetComputeShader("SPECULAR_TRACE");

	GLClasses::Shader& VolumetricsShader = ShaderManager::GetShader("VOLUMETRICS");
	GLClasses::Shader& VolumetricsCompositeShader = ShaderManager::GetShader("VOLUMETRICS_COMPOSITE");

	GLClasses::ComputeShader& CollisionShader = ShaderManager::GetComputeShader("COLLISIONS");

	GLClasses::Shader& CompositeShader = ShaderManager::GetShader("COMPOSITE");
	GLClasses::Shader& PostFXCombineShader = ShaderManager::GetShader("PFX_COMBINE");
	GLClasses::Shader& DOFShader = ShaderManager::GetShader("DOF");

	GLClasses::Shader& GlassDeferredShader = ShaderManager::GetShader("GLASS_DEFERRED");
	GLClasses::Shader& GlassDeferredShaderStochastic = ShaderManager::GetShader("GLASS_DEFERRED_ST");
	GLClasses::Shader& BasicBlitShader = ShaderManager::GetShader("BASIC_BLIT");
	GLClasses::Shader& GenerateHQN = ShaderManager::GetShader("GEN_HQN");
	GLClasses::Shader& ProbeForwardShader = ShaderManager::GetShader("PROBE");

	// Matrices
	glm::mat4 PreviousView;
	glm::mat4 PreviousProjection;
	glm::mat4 View;
	glm::mat4 Projection;
	glm::mat4 InverseView;
	glm::mat4 InverseProjection;

	// Collisions 
	GLuint CollisionQuerySSBO = 0;
	glGenBuffers(1, &CollisionQuerySSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, CollisionQuerySSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(CollisionQuery) * 16, nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	GLuint CollisionResultSSBO = 0;
	glGenBuffers(1, &CollisionResultSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, CollisionResultSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::ivec4) * 16, nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// DOF 
	GLuint DOFSSBO = 0;
	float DOFDATA = 0.;
	glGenBuffers(1, &DOFSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, DOFSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 1, &DOFDATA, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Lens Flare 
	GLuint PlayerSSBO = 0;
	float PlayerInShadow = 0.;
	glGenBuffers(1, &PlayerSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, PlayerSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 1, &PlayerInShadow, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Sphere Lights 
	GLuint SphereLightSSBO = 0;
	std::vector<SphereLight> SphereLights;
	glGenBuffers(1, &SphereLightSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SphereLightSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 8 * 16, &SphereLightSSBO, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Light Culler 
	LightCuller::CreateVolumes();


	// Generate shadow maps
	ShadowHandler::GenerateShadowMaps();

	// Initialize radiance probes
	ProbeGI::Initialize();

	// TAA
	GenerateJitterStuff();

	// Bloom
	const float BloomResolution = 0.25f;
	BloomFBO BloomBufferA(16, 16);
	BloomFBO BloomBufferB(16, 16);
	BloomRenderer::Initialize();

	// Misc 
	GLClasses::Framebuffer* FinalDenoiseBufferPtr = &SpatialBuffers[0];

	// Probe
	ProbeMap PlayerProbe(96);

	// Initialize tonymcmapface tonemapper 
	GLuint TonemapLUT = 0;
	Tonemapper::Initialize("Res/tonymmf.dds", TonemapLUT);

	// Voxelizer
	Voxelizer::CreateVolumes();
	Voxelizer::RecompileShaders();

	while (!glfwWindowShouldClose(app.GetWindow()))
	{
		// Window is minimized.
		if (glfwGetWindowAttrib(app.GetWindow(), GLFW_ICONIFIED)) {
			app.OnUpdate();
			ClearAllBuffers();
			app.FinishFrame();
			GLClasses::DisplayFrameRate(app.GetWindow(), "Candela ");
			continue;
		}

		// Prepare 

		int DebugMode = EditMode ? SelectedDebugView : -1;
		bool DebugModeDefault = DebugMode == -1;
		bool DoDOF_ = DebugModeDefault && DoDOF;
		bool DoDistortion_ = DebugModeDefault && DoDistortion;
		float CAScale_ = DebugModeDefault ? CAScale : 0.0f;

		bool FrameMod2 = app.GetCurrentFrame() % 2 == 0;
		glm::vec3 SunDirection = glm::normalize(_SunDirection);

		bool RENDER_GLASS = RENDER_GLASS_FLAG && (DebugMode < 0);

		InternalRenderResolution = RoundToNearest(InternalRenderResolution, 0.25f);

		// Prepare Intersector
		Intersector.PushEntities(EntityRenderList);
		Intersector.BufferEntities();

		SphereLights.clear();

		for (auto& e : EntityRenderList) {
			if (e->m_IsSphereLight) {

				SphereLight s;
				s.PositionRadius = glm::vec4(glm::vec3(e->m_Model[3]), e->ExtractScale().x);
				s.ColorEmissivitys = glm::vec4(glm::vec3(1.0f), e->m_EmissiveAmount);
				SphereLights.push_back(s);
			}
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, SphereLightSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 8 * SphereLights.size(), SphereLights.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Light Culler 

		if (UpdateLightCullingVolume) {
			LightCuller::GenerateVolume(Camera.GetPosition(), SphereLightSSBO, SphereLights.size());
		}

		// 

		// Resize FBOs
		GBuffers[0].SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);
		GBuffers[1].SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);
		TransparentGBuffer.SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);

		// Transparent 
		TransparentPass.SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);

		// Temporal
		MotionVectors.SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);

		// Lighting
		LightingPass.SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);
		TempFramebuffer.SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);
		
		// Antialiasing
		TAABuffers[0].SetSize(app.GetWidth(), app.GetHeight());
		TAABuffers[1].SetSize(app.GetWidth(), app.GetHeight());
		
		// SS refractions
		SSRefractions.SetSize(app.GetWidth() * InternalRenderResolution / 2, app.GetHeight() * InternalRenderResolution / 2);

		// Diffuse FBOs
		int Denominator = DoCheckering ? 4 : 2;
		DiffuseCheckerboardBuffers[0].SetSize(app.GetWidth() * InternalRenderResolution / Denominator, app.GetHeight() * InternalRenderResolution / 2);
		DiffuseCheckerboardBuffers[1].SetSize(app.GetWidth() * InternalRenderResolution / Denominator, app.GetHeight() * InternalRenderResolution / 2);
		SpecularCheckerboardBuffers[0].SetSize(app.GetWidth() * InternalRenderResolution / Denominator, app.GetHeight() * InternalRenderResolution / 2);
		SpecularCheckerboardBuffers[1].SetSize(app.GetWidth() * InternalRenderResolution / Denominator, app.GetHeight() * InternalRenderResolution / 2);

		// Volumetrics
		VolumetricsCheckerboardBuffers[0].SetSize(app.GetWidth() * InternalRenderResolution / Denominator, app.GetHeight() * InternalRenderResolution / 2);
		VolumetricsCheckerboardBuffers[1].SetSize(app.GetWidth() * InternalRenderResolution / Denominator, app.GetHeight() * InternalRenderResolution / 2);

		// Temporal/Spatial resolve buffers
		SpatialUpscaled.SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);
		CheckerboardUpscaled.SetSize(app.GetWidth() * InternalRenderResolution / 2, app.GetHeight() * InternalRenderResolution / 2);
		SpatialVariance.SetSize(app.GetWidth() * InternalRenderResolution / 2, app.GetHeight() * InternalRenderResolution / 2);
		SpatialBuffers[0].SetSize(app.GetWidth() * InternalRenderResolution / 2, app.GetHeight() * InternalRenderResolution / 2);
		SpatialBuffers[1].SetSize(app.GetWidth() * InternalRenderResolution / 2, app.GetHeight() * InternalRenderResolution / 2);
		TemporalBuffersIndirect[0].SetSize(app.GetWidth() * InternalRenderResolution / 2, app.GetHeight() * InternalRenderResolution / 2);
		TemporalBuffersIndirect[1].SetSize(app.GetWidth() * InternalRenderResolution / 2, app.GetHeight() * InternalRenderResolution / 2);
		VolComposited.SetSize(app.GetWidth() * InternalRenderResolution, app.GetHeight() * InternalRenderResolution);

		// Other post buffers
		Composited.SetSize(app.GetWidth(), app.GetHeight());
		PFXComposited.SetSize(app.GetWidth(), app.GetHeight());
		DOF.SetSize(app.GetWidth() / (PerformanceDOF ? 2 : 1), app.GetHeight() / (PerformanceDOF ? 2 : 1));

		if (app.GetCursorLocked()) {
			DOFFocusPoint = glm::vec2(app.GetWidth() * InternalRenderResolution / 2, app.GetHeight() * InternalRenderResolution / 2);
		}

		BloomBufferA.SetSize(app.GetWidth()  * InternalRenderResolution * BloomResolution, app.GetHeight() * InternalRenderResolution * BloomResolution);
		BloomBufferB.SetSize(app.GetWidth()  * InternalRenderResolution * BloomResolution, app.GetHeight() * InternalRenderResolution * BloomResolution);
		
		ShadowHandler::SetDirectShadowMapRes(ShadowmapRes);

		// Set FBO references
		GLClasses::Framebuffer& GBuffer = FrameMod2 ? GBuffers[0] : GBuffers[1];
		GLClasses::Framebuffer& PrevGBuffer = FrameMod2 ? GBuffers[1] : GBuffers[0];

		GLClasses::Framebuffer& DiffuseTrace = FrameMod2 ? DiffuseCheckerboardBuffers[0] : DiffuseCheckerboardBuffers[1];
		GLClasses::Framebuffer& PreviousDiffuseTrace = FrameMod2 ? DiffuseCheckerboardBuffers[1] : DiffuseCheckerboardBuffers[0];
		
		GLClasses::Framebuffer& SpecularTrace = FrameMod2 ? SpecularCheckerboardBuffers[0] : SpecularCheckerboardBuffers[1];
		GLClasses::Framebuffer& PreviousSpecularTrace = FrameMod2 ? SpecularCheckerboardBuffers[1] : SpecularCheckerboardBuffers[0];
		
		GLClasses::Framebuffer& IndirectTemporal = FrameMod2 ? TemporalBuffersIndirect[0] : TemporalBuffersIndirect[1];
		GLClasses::Framebuffer& PreviousIndirectTemporal = FrameMod2 ? TemporalBuffersIndirect[1] : TemporalBuffersIndirect[0];

		GLClasses::Framebuffer& Volumetrics = FrameMod2 ? VolumetricsCheckerboardBuffers[0] : VolumetricsCheckerboardBuffers[1];
		GLClasses::Framebuffer& PreviousVolumetrics = FrameMod2 ? VolumetricsCheckerboardBuffers[1] : VolumetricsCheckerboardBuffers[0];

		GLClasses::Framebuffer& TAA = FrameMod2 ? TAABuffers[0] : TAABuffers[1];
		GLClasses::Framebuffer& PrevTAA = FrameMod2 ? TAABuffers[1] : TAABuffers[0];

		// App update 
		PreviousProjection = Camera.GetProjectionMatrix();
		PreviousView = Camera.GetViewMatrix();

		// Collide

		if (CollidePlayer) {
			CollisionQuery Query;
			Query.Min = glm::vec4(Camera.GetPosition() - 0.1f, 1.0f);
			Query.Max = glm::vec4(Camera.GetPosition() + 0.1f, 1.0f);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, CollisionQuerySSBO);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(CollisionQuery) * 1, &Query);

			CollisionShader.Use();
			CollisionShader.SetInteger("u_QueryCount", 1);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, CollisionQuerySSBO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, CollisionResultSSBO);
			Intersector.BindEverything(CollisionShader, false);
			glDispatchCompute(1, 1, 1);

			glm::ivec4 Retrieved;
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, CollisionResultSSBO);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::ivec4) * 1, &Retrieved);;
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


			if (app.GetCurrentFrame() % 8 == 0)
				std::cout << "\nPlayer Collision Test Result : " << Retrieved.x << "  " << Retrieved.y << "  " << Retrieved.z << "  " << Retrieved.w << "  ";
		}

		if (false) {
			//std::cout << Physics::CollideBox(Camera.GetPosition() - 0.5f, Camera.GetPosition() + 0.5f, Intersector) << "\n";
		}

		// Physics Simulation
		//Physics::Integrate(EntityRenderList, DeltaTime);

		PrevSunDir = _SunDirection;
		app.OnUpdate(); 
		
		bool UpdatedLightThisFrame = false;

		if (PrevSunDir != _SunDirection) {
			UpdatedLightThisFrame = true;
		}

		// Set matrices
		Projection = Camera.GetProjectionMatrix();
		View = Camera.GetViewMatrix();
		InverseProjection = glm::inverse(Camera.GetProjectionMatrix());
		InverseView = glm::inverse(Camera.GetViewMatrix());

		CommonUniforms UniformBuffer = { View, Projection, InverseView, InverseProjection, PreviousProjection, PreviousView, glm::inverse(PreviousProjection), glm::inverse(PreviousView), (int)app.GetCurrentFrame(), SunDirection};

		// Render shadow maps
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);

		for (int i = 0; i < ShadowmapUpdateRate; i++) {
			ShadowHandler::UpdateDirectShadowMaps((app.GetCurrentFrame() * ShadowmapUpdateRate) + i, Camera.GetPosition(), SunDirection, EntityRenderList, ShadowDistanceMultiplier, ShadowmapUpdateRate);

		}

		ShadowHandler::CalculateClipPlanes(Camera.GetProjectionMatrix());
		
		// Hemispherical SM
		if (DoHSM) {
			ShadowHandler::UpdateSkyShadowMaps(app.GetCurrentFrame(), Camera.GetPosition(), EntityRenderList);
		}

		// Render player probe
		if (RENDER_PLAYER_PROBE) {
			bool dynamic = true;
			if (dynamic) {
				for (int i = 0; i < 6 ; i++)
					RenderProbe(PlayerProbe, i, Camera.GetPosition(), EntityRenderList, ProbeForwardShader);
			}
			
			else {
				RenderProbe(PlayerProbe, app.GetCurrentFrame() % 6, Camera.GetPosition(), EntityRenderList, ProbeForwardShader);
			}
		}

		// Update probes
		if (UpdateIrradianceVolume) {
			ProbeGI::UpdateProbes(app.GetCurrentFrame(), Intersector, UniformBuffer, Skymap.GetID(), FilterIrradianceVolume && !UpdatedLightThisFrame);
		}

		// VOXELIZE

		if (DoVoxelization)
		{
			Voxelizer::Voxelize(Camera.GetPosition(), EntityRenderList);
		}

		////

		// Render GBuffer

		if (DoFaceCulling) {
			glEnable(GL_CULL_FACE);
		}

		else {
			glDisable(GL_CULL_FACE);
		}

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);

		glDisable(GL_BLEND);

		// TAA
		float TAAJitterMultiplier = 1.0f; // 1.0f / InternalRenderResolution;
		glm::mat4 TAAMatrix = DoTAA ? GetTAAJitterMatrix(app.GetCurrentFrame(), GBuffer.GetDimensions() * TAAJitterMultiplier) : glm::mat4(1.0f);

		// Render GBuffer
		GBuffer.Bind();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		GBufferShader.Use();
		GBufferShader.SetMatrix4("u_ViewProjection", TAAMatrix * Camera.GetViewProjection());
		GBufferShader.SetInteger("u_AlbedoMap", 0);
		GBufferShader.SetInteger("u_NormalMap", 1);
		GBufferShader.SetInteger("u_RoughnessMap", 2);
		GBufferShader.SetInteger("u_MetalnessMap", 3);
		GBufferShader.SetInteger("u_MetalnessRoughnessMap", 5);
		GBufferShader.SetBool("u_CatmullRom", HQTextureFiltering);
		GBufferShader.SetBool("u_NormalFix", DoNormalFix);
		GBufferShader.SetVector3f("u_ViewerPosition", Camera.GetPosition());
		GBufferShader.SetFloat("u_RoughnessMultiplier", RoughnessMultiplier);
		GBufferShader.SetFloat("u_ScaleLODBias", floor(log2(InternalRenderResolution)));
		GBufferShader.SetVector2f("u_Dimensions", glm::vec2(GBuffer.GetWidth(), GBuffer.GetHeight()));

		RenderEntityList(EntityRenderList, GBufferShader, false);

		if (!RENDER_GLASS) {
			RenderEntityList(EntityRenderList, GBufferShader, true);
		}
		UnbindEverything();

		// Glass pre-pass

		if (RENDER_GLASS) {
			TransparentGBuffer.Bind();

			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			GBufferTransparentPrepassShader.Use();
			GBufferTransparentPrepassShader.SetMatrix4("u_ViewProjection", TAAMatrix * Camera.GetViewProjection());
			GBufferTransparentPrepassShader.SetInteger("u_AlbedoMap", 0);
			GBufferTransparentPrepassShader.SetInteger("u_NormalMap", 1);
			GBufferTransparentPrepassShader.SetVector3f("u_ViewerPosition", Camera.GetPosition());
			GBufferTransparentPrepassShader.SetVector2f("u_Dimensions", glm::vec2(GBuffer.GetWidth(), GBuffer.GetHeight()));
			GBufferTransparentPrepassShader.SetBool("u_Stochastic", StochasticTransparency && OIT);
			GBufferTransparentPrepassShader.SetBool("u_NormalFix", DoNormalFix);

			SetCommonUniforms<GLClasses::Shader>(GBufferTransparentPrepassShader, UniformBuffer);

			RenderEntityList(EntityRenderList, GBufferTransparentPrepassShader, true);

			TransparentGBuffer.Unbind();
			UnbindEverything();
		}

		// Post processing passes :

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		// Generate High Frequency Normals 

		if (GenerateHighFrequencyNormals) {
			
			GBuffer.Bind();
			GenerateHQN.Use();

			GenerateHQN.SetInteger("u_AlbedoTexture", 0);
			GenerateHQN.SetInteger("u_LowFrequencyNormals", 1);
			GenerateHQN.SetFloat("u_Strength", NormalStrength);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(0));

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

			SetCommonUniforms<GLClasses::Shader>(GenerateHQN, UniformBuffer);

			ScreenQuadVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			ScreenQuadVAO.Unbind();

			GBuffer.Unbind();
		}

		// Refractions

		if (RENDER_GLASS && !OIT) {
			SSRefractions.Bind();
			SSRefractionShader.Use();

			SSRefractionShader.SetInteger("u_RefractDepth", 0);
			SSRefractionShader.SetInteger("u_Normals", 1);
			SSRefractionShader.SetInteger("u_Depth", 2);
			SSRefractionShader.SetBool("u_HQ", HQ_Refractions);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetDepthBuffer());

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetTexture(0));

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

			SetCommonUniforms<GLClasses::Shader>(SSRefractionShader, UniformBuffer);

			ScreenQuadVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			ScreenQuadVAO.Unbind();

			SSRefractions.Unbind();
		}

		// Motion vectors 

		MotionVectors.Bind();
		MotionVectorShader.Use();
		MotionVectorShader.SetInteger("u_Depth", 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

		SetCommonUniforms<GLClasses::Shader>(MotionVectorShader, UniformBuffer);

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		MotionVectors.Unbind();

		// Volumetrics 

		if (DoVolumetrics) {
			VolumetricsTemporal = VolumetricsTemporal && DoTemporal;

			VolumetricsShader.Use();
			Volumetrics.Bind();

			VolumetricsShader.SetVector2f("u_Dims", glm::vec2(Volumetrics.GetWidth(), Volumetrics.GetHeight()));
			VolumetricsShader.SetInteger("u_DepthTexture", 0);
			VolumetricsShader.SetInteger("u_NormalTexture", 1);
			VolumetricsShader.SetInteger("u_TransparentDepth", 15);
			VolumetricsShader.SetInteger("u_Skymap", 2);
			VolumetricsShader.SetInteger("u_Steps", VolumetricsSteps);

			VolumetricsShader.SetVector3f("u_ProbeBoxSize", PROBE_GRID_SIZE);
			VolumetricsShader.SetVector3f("u_ProbeGridResolution", PROBE_GRID_RES);
			VolumetricsShader.SetVector3f("u_ProbeBoxOrigin", ProbeGI::GetProbeBoxOrigin());
			VolumetricsShader.SetInteger("u_ProbeRadiance", 14);
			VolumetricsShader.SetFloat("u_Strength", VolumetricsGlobalStrength);
			VolumetricsShader.SetFloat("u_DStrength", VolumetricsDirectStrength);
			VolumetricsShader.SetFloat("u_IStrength", VolumetricsIndirectStrength);
			VolumetricsShader.SetBool("u_Checker", DoCheckering);
			VolumetricsShader.SetBool("u_CompleteTransmittance", CompleteTransmittance);
			VolumetricsShader.SetBool("u_UniformDensity", UniformDensityFog);

			SetCommonUniforms<GLClasses::Shader>(VolumetricsShader, UniformBuffer);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_CUBE_MAP, Skymap.GetID());

			for (int i = 0; i < 5; i++) {

				const int BindingPointStart = 4;

				std::string Name = "u_ShadowMatrices[" + std::to_string(i) + "]";
				std::string NameClip = "u_ShadowClipPlanes[" + std::to_string(i) + "]";
				std::string NameTex = "u_ShadowTextures[" + std::to_string(i) + "]";

				VolumetricsShader.SetMatrix4(Name, ShadowHandler::GetShadowViewProjectionMatrix(i));
				VolumetricsShader.SetInteger(NameTex, i + BindingPointStart);
				VolumetricsShader.SetFloat(NameClip, ShadowHandler::GetShadowCascadeDistance(i));

				glActiveTexture(GL_TEXTURE0 + i + BindingPointStart);
				glBindTexture(GL_TEXTURE_2D, ShadowHandler::GetDirectShadowmap(i));
			}

			glActiveTexture(GL_TEXTURE14);
			glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeColorTexture());

			glActiveTexture(GL_TEXTURE15);
			glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetDepthBuffer());

			ScreenQuadVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			ScreenQuadVAO.Unbind();

			Volumetrics.Unbind();
		}

		// Indirect diffuse raytracing

		DiffuseShader.Use();
		DiffuseTrace.Bind();

		DiffuseShader.SetVector2f("u_Dims", glm::vec2(DiffuseTrace.GetWidth(), DiffuseTrace.GetHeight()));
		DiffuseShader.SetInteger("u_DepthTexture", 0);
		DiffuseShader.SetInteger("u_NormalTexture", 1);
		DiffuseShader.SetInteger("u_Skymap", 2);
		DiffuseShader.SetInteger("u_BlueNoise", 3);

		DiffuseShader.SetVector3f("u_ProbeBoxSize", PROBE_GRID_SIZE);
		DiffuseShader.SetVector3f("u_ProbeGridResolution", PROBE_GRID_RES);
		DiffuseShader.SetVector3f("u_ProbeBoxOrigin", ProbeGI::GetProbeBoxOrigin());
		DiffuseShader.SetInteger("u_IndirectDiffuse", 11);
		DiffuseShader.SetInteger("u_MotionVectors", 12);
		DiffuseShader.SetInteger("u_SHDataA", 14);
		DiffuseShader.SetInteger("u_SHDataB", 15);
		DiffuseShader.SetInteger("u_Albedo", 16);
		DiffuseShader.SetInteger("u_TransparentDepth", 17);
		DiffuseShader.SetInteger("u_TransparentAlbedo", 18);
		DiffuseShader.SetInteger("u_SecondaryBounces", SecondaryBounces);
		DiffuseShader.SetBool("u_Checker", DoCheckering);
		DiffuseShader.SetBool("u_SecondBounce", DoMultiBounce);
		DiffuseShader.SetBool("u_SecondBounceRT", !DoInfiniteBounceGI);
		DiffuseShader.SetBool("u_IndirectSSCaustics", IndirectSSCaustics);
		DiffuseShader.SetBool("DO_BL_SAMPLING", DO_BL_SAMPLING);

		SetCommonUniforms<GLClasses::ComputeShader>(DiffuseShader, UniformBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_CUBE_MAP, Skymap.GetID());

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, BlueNoiseHR.GetTextureID());

		for (int i = 0; i < 5; i++) {

			const int BindingPointStart = 4;

			std::string Name = "u_ShadowMatrices[" + std::to_string(i) + "]";
			std::string NameClip = "u_ShadowClipPlanes[" + std::to_string(i) + "]";
			std::string NameTex = "u_ShadowTextures[" + std::to_string(i) + "]";

			DiffuseShader.SetMatrix4(Name, ShadowHandler::GetShadowViewProjectionMatrix(i));
			DiffuseShader.SetInteger(NameTex, i + BindingPointStart);
			DiffuseShader.SetFloat(NameClip, ShadowHandler::GetShadowCascadeDistance(i));

			glActiveTexture(GL_TEXTURE0 + i + BindingPointStart);
			glBindTexture(GL_TEXTURE_2D, ShadowHandler::GetDirectShadowmap(i));
		}

		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D, FinalDenoiseBufferPtr->GetTexture(0));

		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_2D, MotionVectors.GetTexture());

		glActiveTexture(GL_TEXTURE14);
		glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeDataTextures().x);

		glActiveTexture(GL_TEXTURE15);
		glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeDataTextures().y);

		glActiveTexture(GL_TEXTURE16);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(0));

		glActiveTexture(GL_TEXTURE17);
		glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetDepthBuffer());
		
		glActiveTexture(GL_TEXTURE18);
		glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetTexture(1));

		Intersector.BindEverything(DiffuseShader, true);
		glBindImageTexture(0, DiffuseTrace.GetTexture(), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
		glDispatchCompute((int)floor(float(DiffuseTrace.GetWidth()) / 16.0f) + 1, (int)(floor(float(DiffuseTrace.GetHeight())) / 16.0f) + 1, 1);

		// Indirect specular raytracing 

		SpecularShader.Use();
		SpecularTrace.Bind();

		SpecularShader.SetInteger("u_Depth", 0);
		SpecularShader.SetInteger("u_HFNormals", 1);
		SpecularShader.SetInteger("u_LFNormals", 2);
		SpecularShader.SetInteger("u_PBR", 3);
		SpecularShader.SetInteger("u_Albedo", 4);
		SpecularShader.SetInteger("u_IndirectDiffuse", 5);
		SpecularShader.SetInteger("u_MotionVectors", 17);
		SpecularShader.SetInteger("u_SkyCube", 12);
		SpecularShader.SetVector2f("u_Dimensions", glm::vec2(SpecularTrace.GetWidth(), SpecularTrace.GetHeight()));
		SpecularShader.SetVector3f("u_ProbeBoxSize", PROBE_GRID_SIZE);
		SpecularShader.SetVector3f("u_ProbeGridResolution", PROBE_GRID_RES);
		SpecularShader.SetVector3f("u_ProbeBoxOrigin", ProbeGI::GetProbeBoxOrigin());
		SpecularShader.SetInteger("u_SHDataA", 14);
		SpecularShader.SetInteger("u_SHDataB", 15);
		SpecularShader.SetInteger("u_BlueNoise", 13);
		SpecularShader.SetBool("u_Checker", DoCheckering);
		SpecularShader.SetBool("u_FullRT", DoFullRTSpecular);
		SpecularShader.SetBool("u_RoughSpec", DoRoughSpecular);
		SpecularShader.SetBool("DO_BL_SAMPLING", DO_BL_SAMPLING);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(1));

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(2));

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(0));

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, FinalDenoiseBufferPtr->GetTexture(0));

		glActiveTexture(GL_TEXTURE17);
		glBindTexture(GL_TEXTURE_2D, MotionVectors.GetTexture());

		for (int i = 0; i < 5; i++) {

			const int BindingPointStart = 6;

			std::string Name = "u_ShadowMatrices[" + std::to_string(i) + "]";
			std::string NameClip = "u_ShadowClipPlanes[" + std::to_string(i) + "]";
			std::string NameTex = "u_ShadowTextures[" + std::to_string(i) + "]";

			SpecularShader.SetMatrix4(Name, ShadowHandler::GetShadowViewProjectionMatrix(i));
			SpecularShader.SetInteger(NameTex, i + BindingPointStart);
			SpecularShader.SetFloat(NameClip, ShadowHandler::GetShadowCascadeDistance(i));

			glActiveTexture(GL_TEXTURE0 + i + BindingPointStart);
			glBindTexture(GL_TEXTURE_2D, ShadowHandler::GetDirectShadowmap(i));
		}

		// 6 - 10 used by shadow textures, start binding further textures from index 12

		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_CUBE_MAP, Skymap.GetID());
		
		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D, BlueNoiseHR.GetTextureID());

		glActiveTexture(GL_TEXTURE14);
		glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeDataTextures().x);

		glActiveTexture(GL_TEXTURE15);
		glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeDataTextures().y);

		SetCommonUniforms<GLClasses::ComputeShader>(SpecularShader, UniformBuffer);

		Intersector.BindEverything(SpecularShader, true);
		glBindImageTexture(0, SpecularTrace.GetTexture(), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
		glDispatchCompute((int)floor(float(SpecularTrace.GetWidth()) / 16.0f) + 1, (int)(floor(float(SpecularTrace.GetHeight())) / 16.0f) + 1, 1);


		// Spatio-temporal Checkerboard reconstruction

		CheckerboardUpscaled.Bind();
		CheckerReconstructShader.Use();

		CheckerReconstructShader.SetInteger("u_Depth", 0);
		CheckerReconstructShader.SetInteger("u_Normals", 1);
		CheckerReconstructShader.SetInteger("u_CurrentFrameTexture", 2);
		CheckerReconstructShader.SetInteger("u_PreviousFrameTexture", 3);
		CheckerReconstructShader.SetInteger("u_PreviousDepth", 4);
		CheckerReconstructShader.SetInteger("u_PreviousNormals", 5);
		CheckerReconstructShader.SetInteger("u_MotionVectors", 6);
		CheckerReconstructShader.SetInteger("u_CurrentFrameSpecular", 7);
		CheckerReconstructShader.SetInteger("u_PreviousFrameSpecular", 8);
		CheckerReconstructShader.SetInteger("u_CurrentFrameVolumetrics", 9);
		CheckerReconstructShader.SetInteger("u_PreviousFrameVolumetrics", 10);
		CheckerReconstructShader.SetBool("u_Enabled", DoCheckering);
		CheckerReconstructShader.SetVector2f("u_Dimensions", glm::vec2(CheckerboardUpscaled.GetWidth(), CheckerboardUpscaled.GetHeight()));

		SetCommonUniforms<GLClasses::Shader>(CheckerReconstructShader, UniformBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, DiffuseTrace.GetTexture());

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, PreviousDiffuseTrace.GetTexture());

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, PrevGBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, PrevGBuffer.GetTexture(3));

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, MotionVectors.GetTexture());

		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, SpecularTrace.GetTexture());

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_2D, PreviousSpecularTrace.GetTexture());

		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_2D, Volumetrics.GetTexture());

		glActiveTexture(GL_TEXTURE10);
		glBindTexture(GL_TEXTURE_2D, PreviousVolumetrics.GetTexture());

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		CheckerboardUpscaled.Unbind();

		// Temporal 

		TemporalFilterShader.Use();
		IndirectTemporal.Bind();

		TemporalFilterShader.SetInteger("u_Depth", 0);
		TemporalFilterShader.SetInteger("u_Normals", 1);
		TemporalFilterShader.SetInteger("u_DiffuseCurrent", 2);
		TemporalFilterShader.SetInteger("u_DiffuseHistory", 3);
		TemporalFilterShader.SetInteger("u_PreviousDepth", 4);
		TemporalFilterShader.SetInteger("u_PreviousNormals", 5);
		TemporalFilterShader.SetInteger("u_MotionVectors", 6);
		TemporalFilterShader.SetInteger("u_Utility", 7);
		TemporalFilterShader.SetInteger("u_MomentsHistory", 8);
		TemporalFilterShader.SetInteger("u_SpecularCurrent", 9);
		TemporalFilterShader.SetInteger("u_SpecularHistory", 10);
		TemporalFilterShader.SetInteger("u_PBR", 11);
		TemporalFilterShader.SetInteger("u_VolumetricsCurrent", 12);
		TemporalFilterShader.SetInteger("u_VolumetricsHistory", 13);
		TemporalFilterShader.SetBool("u_DoVolumetrics", DoVolumetrics && VolumetricsTemporal);
		TemporalFilterShader.SetBool("u_Enabled", DoTemporal);
		TemporalFilterShader.SetBool("u_RoughSpec", DoRoughSpecular);
		TemporalFilterShader.SetBool("u_ClipDiffuse", ClipDiffuse);
		TemporalFilterShader.SetFloat("u_ClipStrength", 0.082f - ClipStrength);

		SetCommonUniforms<GLClasses::Shader>(TemporalFilterShader, UniformBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, CheckerboardUpscaled.GetTexture());

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, PreviousIndirectTemporal.GetTexture());

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, PrevGBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, PrevGBuffer.GetTexture(3));

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, MotionVectors.GetTexture());

		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, PreviousIndirectTemporal.GetTexture(1));

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_2D, PreviousIndirectTemporal.GetTexture(2));

		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_2D, CheckerboardUpscaled.GetTexture(1));

		glActiveTexture(GL_TEXTURE10);
		glBindTexture(GL_TEXTURE_2D, PreviousIndirectTemporal.GetTexture(3));

		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(2));

		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_2D, CheckerboardUpscaled.GetTexture(2));

		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D, PreviousIndirectTemporal.GetTexture(4));

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		IndirectTemporal.Unbind();

		// SVGF 

		{
			SpatialVariance.Bind();
			SpatialVarianceShader.Use();

			SpatialVarianceShader.SetInteger("u_Depth", 0);
			SpatialVarianceShader.SetInteger("u_Normals", 1);
			SpatialVarianceShader.SetInteger("u_Diffuse", 2);
			SpatialVarianceShader.SetInteger("u_FrameCounters", 3);
			SpatialVarianceShader.SetInteger("u_TemporalMoments", 4);
			SpatialVarianceShader.SetBool("u_Enabled", DoSpatial);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, IndirectTemporal.GetTexture());

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, IndirectTemporal.GetTexture(1));

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, IndirectTemporal.GetTexture(2));

			SetCommonUniforms<GLClasses::Shader>(SpatialVarianceShader, UniformBuffer);

			ScreenQuadVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			ScreenQuadVAO.Unbind();

			glUseProgram(0);
		}

		// Atrous passes 
		{
			// We use a small 3x3 kernel, so we use more passes
			// This is actually faster due to fewer cache hits
			const int Passes = 5; 
			const int StepSizes[6] = { 1, 2, 4, 8, 16, 32 };

			for (int Pass = 0; Pass < Passes; Pass++) {

				auto& CurrentBuffer = SpatialBuffers[(Pass % 2 == 0) ? 0 : 1];
				auto& SpatialPrevious = SpatialBuffers[(Pass % 2 == 0) ? 1 : 0];

				FinalDenoiseBufferPtr = &CurrentBuffer;

				bool InitialPass = Pass == 0;

				CurrentBuffer.Bind();
				SpatialFilterShader.Use();

				SpatialFilterShader.SetInteger("u_Depth", 0);
				SpatialFilterShader.SetInteger("u_Normals", 1);
				SpatialFilterShader.SetInteger("u_Diffuse", 2);
				SpatialFilterShader.SetInteger("u_Variance", 3);
				SpatialFilterShader.SetInteger("u_FrameCounters", 4);
				SpatialFilterShader.SetInteger("u_Specular", 5);
				SpatialFilterShader.SetInteger("u_NormalsHF", 6);
				SpatialFilterShader.SetInteger("u_PBR", 7);
				SpatialFilterShader.SetInteger("u_Volumetrics", 8);
				SpatialFilterShader.SetInteger("u_StepSize", StepSizes[Pass]);
				SpatialFilterShader.SetInteger("u_Pass", Pass);
				SpatialFilterShader.SetFloat("u_SpecularBeta", SpecularBeta);
				SpatialFilterShader.SetBool("u_FilterVolumetrics", DoVolumetrics && VolumetricsSpatial);
				SpatialFilterShader.SetBool("u_Enabled", DoSpatial);
				SpatialFilterShader.SetFloat("u_SqrtStepSize", glm::sqrt(float(StepSizes[Pass])));
				SpatialFilterShader.SetFloat("u_PhiLMult", 1.0f/glm::max(SVGFStrictness,0.01f));
				SpatialFilterShader.SetFloat("u_SpecularTransversalWeight", SpecularTransversalWeight);
				SpatialFilterShader.SetBool("u_RoughSpec", DoRoughSpecular);
				SpatialFilterShader.SetBool("u_DoSVGF", DoSVGF);

				SetCommonUniforms<GLClasses::Shader>(SpatialFilterShader, UniformBuffer);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, InitialPass ? SpatialVariance.GetTexture() : SpatialPrevious.GetTexture());

				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, InitialPass ? SpatialVariance.GetTexture(1) : SpatialPrevious.GetTexture(1));

				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, IndirectTemporal.GetTexture(1));

				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, InitialPass ? IndirectTemporal.GetTexture(3) : SpatialPrevious.GetTexture(2));

				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(1));

				glActiveTexture(GL_TEXTURE7);
				glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(2));

				glActiveTexture(GL_TEXTURE8);
				glBindTexture(GL_TEXTURE_2D, InitialPass ? IndirectTemporal.GetTexture(4) : SpatialPrevious.GetTexture(3));

				ScreenQuadVAO.Bind();
				glDrawArrays(GL_TRIANGLES, 0, 6);
				ScreenQuadVAO.Unbind();
			}
		}

		// Spatial Upscale 

		SpatialUpscaleShader.Use();
		SpatialUpscaled.Bind();

		SpatialUpscaleShader.SetInteger("u_Depth", 0);
		SpatialUpscaleShader.SetInteger("u_Normals", 1);

		SpatialUpscaleShader.SetInteger("u_Diffuse", 2);
		SpatialUpscaleShader.SetInteger("u_Specular", 3);
		SpatialUpscaleShader.SetInteger("u_Volumetrics", 4);

		SpatialUpscaleShader.SetInteger("u_PBR", 5);
		SpatialUpscaleShader.SetInteger("u_NormalsHF", 6);
		SpatialUpscaleShader.SetBool("u_Enabled", DoSpatialUpscaling);

		SetCommonUniforms<GLClasses::Shader>(SpatialUpscaleShader, UniformBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, FinalDenoiseBufferPtr->GetTexture(0));

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, FinalDenoiseBufferPtr->GetTexture(2));

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, FinalDenoiseBufferPtr->GetTexture(3));

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(2));

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(1));

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		SpatialUpscaled.Unbind();

		// Light Combiner 

		LightingShader.Use();
		LightingPass.Bind();

		LightingShader.SetInteger("u_AlbedoTexture", 0);
		LightingShader.SetInteger("u_NormalTexture", 1);
		LightingShader.SetInteger("u_PBRTexture", 2);
		LightingShader.SetInteger("u_DepthTexture", 3);
		LightingShader.SetInteger("u_BlueNoise", 5);
		LightingShader.SetInteger("u_Skymap", 6);
		LightingShader.SetInteger("u_IndirectDiffuse", 7);
		LightingShader.SetInteger("u_IndirectSpecular", 8);

		LightingShader.SetMatrix4("u_LightVP",ShadowHandler::GetShadowViewProjectionMatrix(0));
		LightingShader.SetVector2f("u_Dims", glm::vec2(app.GetWidth(), app.GetHeight()));

		LightingShader.SetVector3f("u_ProbeBoxSize", PROBE_GRID_SIZE);
		LightingShader.SetVector3f("u_ProbeGridResolution", PROBE_GRID_RES);
		LightingShader.SetVector3f("u_ProbeBoxOrigin", ProbeGI::GetProbeBoxOrigin());

		LightingShader.SetInteger("u_SHDataA", 15);
		LightingShader.SetInteger("u_SHDataB", 16);
		LightingShader.SetInteger("u_Volumetrics", 17);
		LightingShader.SetInteger("u_NormalLFTexture", 18);
		LightingShader.SetInteger("u_DebugTexture", 19);
		LightingShader.SetInteger("u_ProbePlayer", 20);
		LightingShader.SetInteger("u_ProbePlayerDepth", 21);
		LightingShader.SetInteger("u_TransparentDepth", 23);
		LightingShader.SetInteger("u_DebugMode", DebugMode);

		LightingShader.SetInteger("u_VoxelRange", Voxelizer::GetVolRange());
		LightingShader.SetInteger("u_VoxelVolSize", Voxelizer::GetVolSize());
		LightingShader.SetInteger("u_DoVoxelization", DoVoxelization);

		LightingShader.SetBool("u_DoVolumetrics", DoVolumetrics);
		LightingShader.SetBool("u_DoSSShadow", DoScreenspaceShadow);
		LightingShader.SetVector2f("u_FocusPoint", DOFFocusPoint / glm::vec2(app.GetWidth(), app.GetHeight()));

		LightingShader.SetVector2f("u_ShadowBiasMult", glm::vec2(ShadowNBiasMultiplier,ShadowSBiasMultiplier));

		LightingShader.SetFloat("u_RTAOStrength", RTAOStrength);


		for (int i = 0; i < 6; i++) {
			std::string name = "u_ProbeCapturePoints[" + std::to_string(i) + "]";
			LightingShader.SetVector3f(name.c_str(), PlayerProbe.CapturePoints[i]);
		}

		
		SetCommonUniforms<GLClasses::Shader>(LightingShader, UniformBuffer);

		for (int i = 0; i < 5; i++) {

			const int BindingPointStart = 9;

			std::string Name = "u_ShadowMatrices[" + std::to_string(i) + "]";
			std::string NameClip = "u_ShadowClipPlanes[" + std::to_string(i) + "]";
			std::string NameTex = "u_ShadowTextures[" + std::to_string(i) + "]";

			LightingShader.SetMatrix4(Name, ShadowHandler::GetShadowViewProjectionMatrix(i));
			LightingShader.SetInteger(NameTex, i+BindingPointStart);
			LightingShader.SetFloat(NameClip, ShadowHandler::GetShadowCascadeDistance(i));

			glActiveTexture(GL_TEXTURE0 + i + BindingPointStart);
			glBindTexture(GL_TEXTURE_2D, ShadowHandler::GetDirectShadowmap(i));
		}

		if (DoHSM) {
			for (int i = 0; i < SKY_SHADOWMAP_COUNT; i++) {
				std::string Name = "SkyHemisphericalShadowmaps[" + std::to_string(i) + "]";
				std::string NameM = "u_SkyShadowMatrices[" + std::to_string(i) + "]";
				glProgramUniformHandleui64ARB(LightingShader.GetProgram(), LightingShader.FetchUniformLocation(Name), ShadowHandler::GetSkyShadowmapRef(i).GetHandle());
				LightingShader.SetMatrix4(NameM, ShadowHandler::GetSkyShadowViewProjectionMatrix(i));
			}
		}


		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(0));

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(1));

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(2));

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, BlueNoise.GetTextureID());

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_CUBE_MAP, Skymap.GetID());
		
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, SpatialUpscaled.GetTexture(0));

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_2D, SpatialUpscaled.GetTexture(1));
		

		// 8 - 13 occupied by shadow textures 

		glActiveTexture(GL_TEXTURE15);
		glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeDataTextures().x);

		glActiveTexture(GL_TEXTURE16);
		glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeDataTextures().y);
		

		glActiveTexture(GL_TEXTURE17);
		glBindTexture(GL_TEXTURE_2D, SpatialUpscaled.GetTexture(2));

		glActiveTexture(GL_TEXTURE18);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetTexture(3));
		
		if (DebugFBO) {
			glActiveTexture(GL_TEXTURE19);
			glBindTexture(GL_TEXTURE_2D, DebugFBO->GetTexture(FBODebugAttachment));
		}

		if (RENDER_PLAYER_PROBE) {
			glActiveTexture(GL_TEXTURE20);
			glBindTexture(GL_TEXTURE_CUBE_MAP, PlayerProbe.m_CubemapTexture);

			glActiveTexture(GL_TEXTURE21);
			glBindTexture(GL_TEXTURE_CUBE_MAP, PlayerProbe.m_DepthCubemap);

			glActiveTexture(GL_TEXTURE22);
			glBindTexture(GL_TEXTURE_CUBE_MAP, PlayerProbe.NormalPBRPackedCubemap);
		}

		glActiveTexture(GL_TEXTURE23);
		glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetDepthBuffer());


		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ProbeGI::GetProbeDataSSBO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, DOFSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, PlayerSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, SphereLightSSBO);

		glBindImageTexture(0, GBuffer.GetTexture(3), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
		//glBindImageTexture(4, Voxelizer::GetVolume(), 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
		glBindImageTexture(4, LightCuller::GetVolume(0), 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		LightingPass.Unbind();

		// find center depth 
		float DownloadedCenterDepth = 0.0f;

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, DOFSSBO);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(float), &DownloadedCenterDepth);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		FocusDepthSmooth = glm::mix(DownloadedCenterDepth, FocusDepthSmooth, 0.75f);

		// find avg player shadow
		float DownloadedPlayerShadow = 0.0f;

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, PlayerSSBO);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(float), &DownloadedPlayerShadow);;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		PlayerShadowSmooth = glm::mix(DownloadedPlayerShadow, PlayerShadowSmooth, 0.6f);

		// Transparent Forward pass 

		if (RENDER_GLASS)
		{
			glDisable(GL_BLEND);

			if (OIT && !StochasticTransparency) {

				glDisable(GL_DEPTH_TEST);
				glDisable(GL_CULL_FACE);
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				glBlendFunci(0, GL_ONE, GL_ONE);
				glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
				glBlendEquation(GL_FUNC_ADD);

				TransparentForwardShader.Use();
				TransparentPass.Bind();
				glClearBufferfv(GL_COLOR, 0, glm::value_ptr(glm::vec4(0.0f)));
				glClearBufferfv(GL_COLOR, 1, glm::value_ptr(glm::vec4(1.0f)));

				TransparentForwardShader.SetMatrix4("u_ViewProjection", TAAMatrix* Camera.GetViewProjection());
				TransparentForwardShader.SetInteger("u_AlbedoMap", 0);
				TransparentForwardShader.SetInteger("u_NormalMap", 1);
				TransparentForwardShader.SetInteger("u_RefractionData", 8);
				TransparentForwardShader.SetInteger("u_OpaqueLighting", 9);
				TransparentForwardShader.SetVector3f("u_ViewerPosition", Camera.GetPosition());
				TransparentForwardShader.SetVector2f("u_Dimensions", glm::vec2(TransparentPass.GetWidth(), TransparentPass.GetHeight()));
				TransparentForwardShader.SetVector3f("u_ProbeBoxSize", PROBE_GRID_SIZE);
				TransparentForwardShader.SetVector3f("u_ProbeGridResolution", PROBE_GRID_RES);
				TransparentForwardShader.SetVector3f("u_ProbeBoxOrigin", ProbeGI::GetProbeBoxOrigin());

				TransparentForwardShader.SetInteger("u_RadianceCache", 14);
				TransparentForwardShader.SetBool("u_NormalFix", DoNormalFix);


				glActiveTexture(GL_TEXTURE14);
				glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeColorTexture());

				SetCommonUniforms<GLClasses::Shader>(TransparentForwardShader, UniformBuffer);

				glActiveTexture(GL_TEXTURE8);
				glBindTexture(GL_TEXTURE_2D, SSRefractions.GetTexture());

				glActiveTexture(GL_TEXTURE9);
				glBindTexture(GL_TEXTURE_2D, LightingPass.GetTexture());

				RenderEntityList(EntityRenderList, TransparentForwardShader, true);

				TransparentPass.Unbind();

				// OIT Composite 
				glDisable(GL_CULL_FACE);
				glDepthFunc(GL_ALWAYS);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				LightingPass.Bind();

				OITCompositeShader.Use();

				OITCompositeShader.SetInteger("u_Blend", 0);
				OITCompositeShader.SetInteger("u_Revealage", 1);
				OITCompositeShader.SetInteger("u_OpaqueDepth", 2);
				OITCompositeShader.SetInteger("u_TransparentDepth", 3);

				SetCommonUniforms<GLClasses::Shader>(OITCompositeShader, UniformBuffer);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, TransparentPass.GetTexture(0));

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, TransparentPass.GetTexture(1));

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetDepthBuffer());

				ScreenQuadVAO.Bind();
				glDrawArrays(GL_TRIANGLES, 0, 6);
				ScreenQuadVAO.Unbind();

				LightingPass.Unbind();
			}

			else if (OIT && StochasticTransparency) {


				glDisable(GL_CULL_FACE);
				glDisable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);

				// Copy lighting data
				BasicBlitShader.Use();
				TempFramebuffer.Bind();

				BasicBlitShader.SetInteger("u_Input", 0);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, LightingPass.GetTexture(0));

				ScreenQuadVAO.Bind();
				glDrawArrays(GL_TRIANGLES, 0, 6);
				ScreenQuadVAO.Unbind();

				TempFramebuffer.Unbind();

				// Shade glass on top of opaque geometry
				GlassDeferredShaderStochastic.Use();
				LightingPass.Bind();

				GlassDeferredShaderStochastic.SetInteger("u_AlbedoData", 1);
				GlassDeferredShaderStochastic.SetInteger("u_NormalData", 2);
				GlassDeferredShaderStochastic.SetInteger("u_RefractionData", 3);
				GlassDeferredShaderStochastic.SetInteger("u_OpaqueLighting", 4);
				GlassDeferredShaderStochastic.SetInteger("u_OpaqueDepth", 5);
				GlassDeferredShaderStochastic.SetInteger("u_TransparentDepth", 6);

				GlassDeferredShaderStochastic.SetVector3f("u_ProbeBoxSize", PROBE_GRID_SIZE);
				GlassDeferredShaderStochastic.SetVector3f("u_ProbeGridResolution", PROBE_GRID_RES);
				GlassDeferredShaderStochastic.SetVector3f("u_ProbeBoxOrigin", ProbeGI::GetProbeBoxOrigin());

				GlassDeferredShaderStochastic.SetInteger("u_RadianceCache", 14);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetTexture(1));

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetTexture(0));

				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, SSRefractions.GetTexture());

				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, TempFramebuffer.GetTexture());

				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetDepthBuffer());

				glActiveTexture(GL_TEXTURE12);
				glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeDataTextures().x);

				glActiveTexture(GL_TEXTURE13);
				glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeDataTextures().y);

				glActiveTexture(GL_TEXTURE14);
				glBindTexture(GL_TEXTURE_3D, ProbeGI::GetProbeColorTexture());

				SetCommonUniforms<GLClasses::Shader>(GlassDeferredShaderStochastic, UniformBuffer);

				ScreenQuadVAO.Bind();
				glDrawArrays(GL_TRIANGLES, 0, 6);
				ScreenQuadVAO.Unbind();

				LightingPass.Unbind();

			}


			else {

				glDisable(GL_CULL_FACE);
				glDisable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);

				// Copy lighting data
				BasicBlitShader.Use();
				TempFramebuffer.Bind();

				BasicBlitShader.SetInteger("u_Input", 0);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, LightingPass.GetTexture(0));

				ScreenQuadVAO.Bind();
				glDrawArrays(GL_TRIANGLES, 0, 6);
				ScreenQuadVAO.Unbind();

				TempFramebuffer.Unbind();

				// Shade glass on top of opaque geometry
				GlassDeferredShader.Use();
				LightingPass.Bind();

				GlassDeferredShader.SetInteger("u_AlbedoData", 1);
				GlassDeferredShader.SetInteger("u_NormalData", 2);
				GlassDeferredShader.SetInteger("u_RefractionData", 3);
				GlassDeferredShader.SetInteger("u_OpaqueLighting", 4);
				GlassDeferredShader.SetInteger("u_OpaqueDepth", 5);
				GlassDeferredShader.SetInteger("u_TransparentDepth", 6);
				GlassDeferredShader.SetInteger("u_Vol", 7);
				GlassDeferredShader.SetBool("u_DoVol", DoVolumetrics);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetTexture(1));

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetTexture(0));

				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, SSRefractions.GetTexture());

				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, TempFramebuffer.GetTexture());

				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());


				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_2D, TransparentGBuffer.GetDepthBuffer());

				glActiveTexture(GL_TEXTURE7);
				glBindTexture(GL_TEXTURE_2D, SpatialUpscaled.GetTexture(2));

				SetCommonUniforms<GLClasses::Shader>(GlassDeferredShader, UniformBuffer);

				ScreenQuadVAO.Bind();
				glDrawArrays(GL_TRIANGLES, 0, 6);
				ScreenQuadVAO.Unbind();

				LightingPass.Unbind();
			}
		}

		// Composite volumetrics (use post fx composite buffer for perf sake)

		VolComposited.Bind();

		VolumetricsCompositeShader.Use();

		VolumetricsCompositeShader.SetInteger("u_Lighting", 0);
		VolumetricsCompositeShader.SetInteger("u_Volumetrics", 1);
		VolumetricsCompositeShader.SetBool("u_VolumetricsEnabled", DoVolumetrics && (DebugMode < 0 || DebugMode == 5));
		VolumetricsCompositeShader.SetBool("IndirectSSCaustics", IndirectSSCaustics && RENDER_GLASS);
		VolumetricsCompositeShader.SetFloat("u_InternalRenderResolution", InternalRenderResolution);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, LightingPass.GetTexture());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, SpatialUpscaled.GetTexture(2));

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		VolComposited.Unbind();

		// Reset

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		
		// Temporal Anti Aliasing 

		TAAShader.Use();
		TAA.Bind();

		TAAShader.SetInteger("u_CurrentColorTexture", 0);
		TAAShader.SetInteger("u_PreviousColorTexture", 1);
		TAAShader.SetInteger("u_DepthTexture", 2);
		TAAShader.SetInteger("u_PreviousDepthTexture", 3);
		TAAShader.SetInteger("u_MotionVectors", 4);
		TAAShader.SetInteger("u_Volumetrics", 5);
		TAAShader.SetBool("u_Enabled", DoTAA);
		TAAShader.SetBool("u_FSRU", FSR);
		TAAShader.SetFloat("u_InternalRenderResolution", InternalRenderResolution);
		TAAShader.SetFloat("u_TAAStrengthMultiplier", DebugMode >= 0 ? 0.0f : TAAStrengthMultiplier);
		TAAShader.SetFloat("u_TAAClipBias", TAAClipBias);
		TAAShader.SetVector2f("u_CurrentJitter", GetTAAJitter(app.GetCurrentFrame()));

		SetCommonUniforms<GLClasses::Shader>(TAAShader, UniformBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, VolComposited.GetTexture());

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, PrevTAA.GetTexture());

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, PrevGBuffer.GetDepthBuffer());

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, MotionVectors.GetTexture());

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, SpatialUpscaled.GetTexture(2));

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		// Bloom 

		GLuint BrightTex = 0;

		if (DoBloom) {
			BloomRenderer::RenderBloom(TAA.GetTexture(), GBuffer.GetTexture(3), BloomBufferA, BloomBufferB, BrightTex, true);
		}

		glm::vec2 SunScreenspaceCoord;
		{	
			glm::vec4 ViewSpace = Camera.GetViewMatrix() * glm::vec4(-SunDirection * 10000.0f, 1.0f);
			glm::vec4 Projected = Camera.GetProjectionMatrix() * ViewSpace;
			Projected.x /= Projected.w;
			Projected.y /= Projected.w;
			Projected.z /= Projected.w;
			Projected = Projected * 0.5f + 0.5f;

			SunScreenspaceCoord = glm::vec2(Projected.x, Projected.y) - 0.5f;
			SunScreenspaceCoord.x *= float(app.GetWidth()) / float(app.GetHeight());

		}

		// Combine Post FX (excluding DOF.)

		PFXComposited.Bind();
		PostFXCombineShader.Use();

		PostFXCombineShader.SetInteger("u_MainTexture", 0);

		PostFXCombineShader.SetInteger("u_BloomMips[0]", 1);
		PostFXCombineShader.SetInteger("u_BloomMips[1]", 2);
		PostFXCombineShader.SetInteger("u_BloomMips[2]", 3);
		PostFXCombineShader.SetInteger("u_BloomMips[3]", 4);
		PostFXCombineShader.SetInteger("u_BloomMips[4]", 5);
		PostFXCombineShader.SetInteger("u_BloomBrightTexture", 6);
		PostFXCombineShader.SetInteger("u_Depth", 7);
		PostFXCombineShader.SetInteger("u_BlueNoise", 8);
		PostFXCombineShader.SetBool("u_BloomEnabled", DoBloom && DebugMode==-1);
		PostFXCombineShader.SetBool("u_FXAAEnabled", DoFXAA && DebugMode==-1);
		PostFXCombineShader.SetFloat("u_FXAAAmt", FXAAStrength);
		PostFXCombineShader.SetFloat("u_CAScale", CAScale_);
		PostFXCombineShader.SetFloat("u_PlayerShadow", PlayerShadowSmooth);
		PostFXCombineShader.SetFloat("u_LensFlareStrength", DebugMode >= 0 ? 0.0f : LensFlareStrength);
		PostFXCombineShader.SetVector2f("u_SunScreenPosition", SunScreenspaceCoord);
		PostFXCombineShader.SetFloat("u_InternalRenderResolution", InternalRenderResolution);

		SetCommonUniforms<GLClasses::Shader>(PostFXCombineShader, UniformBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TAA.GetTexture(0));

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, BloomBufferA.m_Mips[0]);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, BloomBufferA.m_Mips[1]);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, BloomBufferA.m_Mips[2]);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, BloomBufferA.m_Mips[3]);

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, BloomBufferA.m_Mips[4]);

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, BrightTex);

		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());
		
		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_2D, BlueNoise.GetTextureID());

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		PFXComposited.Unbind();

		// DOF

		if (DoDOF_) {

			DOFShader.Use();
			DOF.Bind();

			DOFShader.SetInteger("u_DepthTexture", 0);
			DOFShader.SetInteger("u_Input", 1);
			DOFShader.SetBool("u_HQ", HQDOFBlur);
			DOFShader.SetBool("u_PerformanceDOF", PerformanceDOF);

			DOFShader.SetVector2f("u_FocusPoint", DOFFocusPoint / glm::vec2(app.GetWidth(), app.GetHeight()));
			DOFShader.SetFloat("u_zVNear", Camera.GetNearPlane());
			DOFShader.SetFloat("u_zVFar", Camera.GetFarPlane());
			DOFShader.SetFloat("u_FocusDepth", FocusDepthSmooth);
			DOFShader.SetFloat("u_BlurRadius", DOFBlurRadius);
			DOFShader.SetFloat("u_DOFScale", DOFScale);

			SetCommonUniforms<GLClasses::Shader>(DOFShader, UniformBuffer);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, GBuffer.GetDepthBuffer());

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, PFXComposited.GetTexture(0));

			ScreenQuadVAO.Bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			ScreenQuadVAO.Unbind();

			DOF.Unbind();
		}

		// Tonemap

		Composited.Bind();

		CompositeShader.Use();
		CompositeShader.SetInteger("u_MainTexture", 0);
		CompositeShader.SetInteger("u_DOF", 1);
		CompositeShader.SetInteger("u_TonyMcMapFaceLUT", 2);
		CompositeShader.SetInteger("u_DebugTexture", 8);
		CompositeShader.SetInteger("u_SelectedTonemap", SelectedTonemap);
		CompositeShader.SetBool("u_DOFEnabled", DoDOF_);
		CompositeShader.SetFloat("u_FocusDepth", FocusDepthSmooth);
		CompositeShader.SetBool("u_PerformanceDOF", PerformanceDOF);
		CompositeShader.SetFloat("u_GrainStrength", GrainStrength);
		CompositeShader.SetFloat("u_DOFScale", DOFScale);
		CompositeShader.SetFloat("u_Exposure", ExposureMultiplier);
		CompositeShader.SetFloat("u_PurkinjeStrength", PurkinjeStrength);


		SetCommonUniforms<GLClasses::Shader>(CompositeShader, UniformBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, PFXComposited.GetTexture(0));

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, DOF.GetTexture());	
		
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_3D, TonemapLUT);


		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// CAS + Gamma correct 

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, app.GetWidth(), app.GetHeight());

		CASShader.Use();

		CASShader.SetBool("u_Enabled", DoCAS);
		CASShader.SetBool("u_DoDistortion", DoDistortion_);
		CASShader.SetFloat("u_GrainStrength", GrainStrength);
		CASShader.SetFloat("u_DistortionK", DistortionK);
		CASShader.SetFloat("u_RenderScale", InternalRenderResolution);

		CASShader.SetBool("u_DebugFBOMode", EditMode && FBODebugMode);
		CASShader.SetBool("u_DebugTexValid", EditMode && FBODebugMode && FBODebugAttachment > -1 && DebugFBO && FBODebugID >= 0);
		CASShader.SetInteger("u_DebugTexture", 8);

		SetCommonUniforms<GLClasses::Shader>(CASShader, UniformBuffer);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Composited.GetTexture(0));

		if (DebugFBO) {
			glActiveTexture(GL_TEXTURE8);
			glBindTexture(GL_TEXTURE_2D, DebugFBO->GetTexture(FBODebugAttachment));
		}

		ScreenQuadVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ScreenQuadVAO.Unbind();

		// Finish 

		glFinish();
		app.FinishFrame();

		CurrentTime = glfwGetTime();
		DeltaTime = CurrentTime - Frametime;
		Frametime = glfwGetTime();

		if (app.GetCurrentFrame() > 4) {
			int GraphIdx = app.GetCurrentFrame() % 1024;
			GraphX[GraphIdx] = glfwGetTime();
			GraphY[GraphIdx] = DeltaTime;
		}

		if (app.GetCurrentFrame() > 4 && WindowResizedThisFrame) {
			WindowResizedThisFrame = false;
			ClearAllBuffers();
		}

		GLClasses::DisplayFrameRate(app.GetWindow(), EditMode ? "Candela | Edit Mode | " : "Candela | ");
	}
}

// End.