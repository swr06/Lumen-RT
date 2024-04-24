#pragma once 

#include <glm/glm.hpp>
#include <iostream>

namespace Candela {
	 
	class SphereLight 
	{
	public : 
		glm::vec4 PositionRadius;
		glm::vec4 ColorEmissivitys;  
	};

}