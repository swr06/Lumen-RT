#pragma once

#include <iostream>

#include <glm/glm.hpp>

#include <glad/glad.h>

#include "GLClasses/ComputeShader.h"

#include "Entity.h"

namespace Candela {
	namespace LightCuller {

		void CreateVolumes();
		void RecompileShaders();
		void GenerateVolume(glm::vec3 Position, GLuint SphereLightSSBO);
		GLuint GetVolume();
		int GetVolSize();
		int GetVolRange();
	}
}