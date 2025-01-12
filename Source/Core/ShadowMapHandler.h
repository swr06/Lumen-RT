#pragma once 

#include "Shadowmap.h"
#include "ShadowRenderer.h"

#include "SkyShadowMap.h"

#include "Macros.h"

#include <iostream>

namespace Candela {
	
	namespace ShadowHandler {

		void GenerateShadowMaps();
		
		void UpdateDirectShadowMaps(int Frame, const glm::vec3& Origin, const glm::vec3& Direction, const std::vector<Entity*> Entities, float DistanceMultiplier, int);
		GLuint GetDirectShadowmap(int n);

		void UpdateSkyShadowMaps(int Frame, const glm::vec3& Origin, const std::vector<Entity*> Entities);
		GLuint GetSkyShadowmap(int n);
		const Candela::Shadowmap& GetSkyShadowmapRef(int n);

		void SetDirectShadowMapRes(int r);

		glm::mat4 GetShadowViewMatrix(int n);
		glm::mat4 GetShadowProjectionMatrix(int n);
		glm::mat4 GetShadowViewProjectionMatrix(int n);

		glm::mat4 GetSkyShadowViewMatrix(int n);
		glm::mat4 GetSkyShadowProjectionMatrix(int n);
		glm::mat4 GetSkyShadowViewProjectionMatrix(int n);

		void CalculateClipPlanes(const glm::mat4& Projection);
		float GetShadowCascadeDistance(int n);
	}

}