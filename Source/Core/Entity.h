#pragma once

#include "Object.h"

#include <iostream>

#include "PhysicsObject.h"

namespace Candela
{

	class Entity
	{
	public : 
		Entity(Object* object);

		~Entity();

		glm::vec3 ExtractScale() {
			glm::vec3 scale;
			scale.x = glm::length(glm::vec3(m_Model[0][0], m_Model[0][1], m_Model[0][2]));
			scale.y = glm::length(glm::vec3(m_Model[1][0], m_Model[1][1], m_Model[1][2]));
			scale.z = glm::length(glm::vec3(m_Model[2][0], m_Model[2][1], m_Model[2][2]));
			return scale;
		}

		//Entity(const Entity&) = delete;
		//Entity operator=(Entity const&) = delete;
		//
		//Entity(Entity&& v) : m_Object(v.m_Object)
		//{
		//	m_EmissiveAmount = v.m_EmissiveAmount;
		//	m_EntityRoughness = v.m_EntityRoughness;
		//	m_EntityMetalness = v.m_EntityMetalness;
		//	m_Model = v.m_Model;
		//}

		Object* const m_Object;
		glm::mat4 m_Model;

		float m_EmissiveAmount = 0.0f;
		float m_EntityRoughness = 0.75f;
		float m_EntityMetalness = 0.0f;
		float m_EntityRoughnessMultiplier = 1.0f;

		// 1.0 -> Completely translucent
		// 0.0 -> Opaque
		float m_TranslucencyAmount = 0.0f;

		Physics::PhysicsObject m_PhysicsObject;

		bool m_IsPhysicsObject = false;
		bool m_UseAlbedoMap = true;
		bool m_UsePBRMap = true;

		bool m_IsSphereLight = false;
	};
}