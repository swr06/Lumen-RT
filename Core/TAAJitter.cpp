#include "TAAJitter.h"

static glm::vec2 HaltonSequenceData[24];

// From inside's talk on temporal anti aliasing
float HaltonSequence(int Prime, int index) 
{
	float r = 0.0f;
	float f = 1.0f;
	int i = index;

	while (i > 0)
	{
		f /= Prime;
		r += f * (i % Prime);
		i = (int)std::floor(i / float(Prime));
	}
	return r;
}

void Lumen::GenerateJitterStuff()
{
	for (int i = 0; i < 32; i++) 
	{
		HaltonSequenceData[i].x = HaltonSequence(2, i + 1);
		HaltonSequenceData[i].y = HaltonSequence(3, i + 1);
	}
}

glm::vec2 Lumen::GetTAAJitter(int CurrentFrame)
{
	glm::vec2 Jitter = HaltonSequenceData[CurrentFrame % 32];
	return Jitter;
}

// Creates the jitter matrix
glm::mat4 Lumen::GetTAAJitterMatrix(int CurrentFrame, const glm::vec2& resolution)
{
	glm::vec2 Jitter = HaltonSequenceData[CurrentFrame % 32];
	glm::vec2 TexelSize = 1.0f / glm::vec2(resolution);
	return glm::translate(glm::mat4(1.0f), glm::vec3((2.0 * Jitter.x - 1.0) * TexelSize.x, (2.0 * Jitter.y - 1.0) * TexelSize.y, 0.0f));
}