#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Object.h"
#include "Entity.h"
#include "GLClasses/Shader.h"

#include "FpsCamera.h"

#include "Frustum.h"

namespace Candela {

	void RenderEntity(Entity& entity, GLClasses::Shader& shader, Frustum& frustum, bool fcull, int entity_num = 0, bool transparent_pass = false);
	void RenderEntityV(Entity& entity, GLClasses::Shader& shader);
	uint64_t QueryPolygonCount();
	void ResetPolygonCount();
}