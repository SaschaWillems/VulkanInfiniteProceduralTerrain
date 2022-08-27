/*
* View frustum culling class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <math.h>
#include <glm/glm.hpp>

namespace vks
{
	class Frustum
	{
	public:
		enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
		glm::vec4 planes[6];
		glm::vec4 corners[8];

		void update(glm::mat4 matrix)
		{
			planes[LEFT].x = matrix[0].w + matrix[0].x;
			planes[LEFT].y = matrix[1].w + matrix[1].x;
			planes[LEFT].z = matrix[2].w + matrix[2].x;
			planes[LEFT].w = matrix[3].w + matrix[3].x;

			planes[RIGHT].x = matrix[0].w - matrix[0].x;
			planes[RIGHT].y = matrix[1].w - matrix[1].x;
			planes[RIGHT].z = matrix[2].w - matrix[2].x;
			planes[RIGHT].w = matrix[3].w - matrix[3].x;

			planes[TOP].x = matrix[0].w - matrix[0].y;
			planes[TOP].y = matrix[1].w - matrix[1].y;
			planes[TOP].z = matrix[2].w - matrix[2].y;
			planes[TOP].w = matrix[3].w - matrix[3].y;

			planes[BOTTOM].x = matrix[0].w + matrix[0].y;
			planes[BOTTOM].y = matrix[1].w + matrix[1].y;
			planes[BOTTOM].z = matrix[2].w + matrix[2].y;
			planes[BOTTOM].w = matrix[3].w + matrix[3].y;

			planes[BACK].x = matrix[0].w + matrix[0].z;
			planes[BACK].y = matrix[1].w + matrix[1].z;
			planes[BACK].z = matrix[2].w + matrix[2].z;
			planes[BACK].w = matrix[3].w + matrix[3].z;

			planes[FRONT].x = matrix[0].w - matrix[0].z;
			planes[FRONT].y = matrix[1].w - matrix[1].z;
			planes[FRONT].z = matrix[2].w - matrix[2].z;
			planes[FRONT].w = matrix[3].w - matrix[3].z;

			for (auto i = 0; i < 6; i++)
			{
				float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
				planes[i] /= length;
			}

			const glm::vec4 v[] = {
				glm::vec4(-1, -1, -1,  1),  glm::vec4(1, -1, -1,  1),
				glm::vec4(1,  1, -1,  1),  glm::vec4(-1,  1, -1,  1),
				glm::vec4(-1, -1,  1,  1),  glm::vec4(1, -1,  1,  1),
				glm::vec4(1,  1,  1,  1),  glm::vec4(-1,  1,  1,  1)
			};
			const glm::mat4 inv = glm::inverse(matrix);
			for (auto i = 0; i < 8; i++) {
				const glm::vec4 q = inv * v[i];
				corners[i] = q / q.w;
			}

		}
		
		bool checkSphere(glm::vec3 pos, float radius)
		{
			for (auto i = 0; i < 6; i++)
			{
				if ((planes[i].x * pos.x) + (planes[i].y * pos.y) + (planes[i].z * pos.z) + planes[i].w <= -radius)
				{
					return false;
				}
			}
			return true;
		}

		bool checkBox(glm::vec3 pos, glm::vec3 min, glm::vec3 max)
		{
			// https://iquilezles.org/articles/frustumcorrect/
			
			// Check box outside/inside of frustum
			for (int i = 0; i < 6; i++)
			{
				int out = 0;
				out += ((glm::dot(planes[i], glm::vec4(min.x, min.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
				out += ((glm::dot(planes[i], glm::vec4(max.x, min.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
				out += ((glm::dot(planes[i], glm::vec4(min.x, max.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
				out += ((glm::dot(planes[i], glm::vec4(max.x, max.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
				out += ((glm::dot(planes[i], glm::vec4(min.x, min.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
				out += ((glm::dot(planes[i], glm::vec4(max.x, min.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
				out += ((glm::dot(planes[i], glm::vec4(min.x, max.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
				out += ((glm::dot(planes[i], glm::vec4(max.x, max.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
				if (out == 8) return false;
			}

			// Check frustum outside/inside box
			int out;
			out = 0; for (int i = 0; i < 8; i++) out += ((corners[i].x > max.x) ? 1 : 0); if (out == 8) return false;
			out = 0; for (int i = 0; i < 8; i++) out += ((corners[i].x < min.x) ? 1 : 0); if (out == 8) return false;
			out = 0; for (int i = 0; i < 8; i++) out += ((corners[i].y > max.y) ? 1 : 0); if (out == 8) return false;
			out = 0; for (int i = 0; i < 8; i++) out += ((corners[i].y < min.y) ? 1 : 0); if (out == 8) return false;
			out = 0; for (int i = 0; i < 8; i++) out += ((corners[i].z > max.z) ? 1 : 0); if (out == 8) return false;
			out = 0; for (int i = 0; i < 8; i++) out += ((corners[i].z < min.z) ? 1 : 0); if (out == 8) return false;

			return true;
		}

	};
}
