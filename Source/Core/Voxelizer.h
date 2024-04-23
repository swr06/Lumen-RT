#pragma once

#include <iostream>

#include <glm/glm.hpp>

#include <glad/glad.h>

#include "GLClasses/ComputeShader.h"

#include "Entity.h"

namespace Candela {
	namespace Voxelizer {

		void CreateVolumes();
		void Voxelize(glm::vec3 Position, const std::vector<Entity*>& EntityList);
		void RecompileShaders();
		GLuint GetVolume();
		int GetVolSize();
		int GetVolRange();
	}
}