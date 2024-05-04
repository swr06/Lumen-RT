#pragma once 

#include <iostream>

#include <glm/glm.hpp>

#include <glad/glad.h>

#include "BVH/Intersector.h"

#include "Utility.h"

#include "FpsCamera.h"

#include "ShadowMapHandler.h"

namespace Candela {

	namespace ProbeGI {

		void Initialize();

		void UpdateProbes(int Frame, RayIntersector<BVH::StacklessTraversalNode>& Intersector, CommonUniforms& uniforms, GLuint Skymap, bool Temporal, int);
		void UpdateProbes(int Frame, RayIntersector<BVH::StackTraversalNode>& Intersector, CommonUniforms& uniforms, GLuint Skymap, bool Temporal, int);
		glm::vec3 GetProbeBoxOrigin();
		GLuint GetProbeDataSSBO();
		glm::uvec2 GetProbeDataTextures();
		GLuint GetProbeColorTexture();
		//GLuint GetVoxelVolume();
	}

}

