/*
* Basic camera class
*
* Copyright (C) 2016-2022 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
private:
	float fov;
	float znear, zfar;

	const glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);

	void updateViewMatrix()
	{
		if (type == CameraType::firstperson)
		{
			matrices.view = glm::lookAt(position, position + frontVector(), upVector);
		}
		else
		{
			// @todo
		}

		updated = true;
	};
public:
	enum CameraType { lookat, firstperson };
	CameraType type = CameraType::lookat;

	glm::vec3 position = glm::vec3();

	float yaw = 0.0f;
	float pitch = 0.0f;

	float rotationSpeed = 1.0f;
	float movementSpeed = 1.0f;
	float currMovementSpeed = 1.0f;

	bool updated = false;

	struct
	{
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
		bool shift = false;
	} keys;

	glm::vec3 frontVector()
	{
		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		return glm::normalize(front);
	}

	bool moving()
	{
		return keys.left || keys.right || keys.up || keys.down;
	}

	float getNearClip() { 
		return znear;
	}

	float getFarClip() {
		return zfar;
	}

	void setPerspective(float fov, float aspect, float znear, float zfar)
	{
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	};

	void updateAspectRatio(float aspect)
	{
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	}

	void setPosition(glm::vec3 position)
	{
		this->position = position;
		updateViewMatrix();
	}

	void setRotation(glm::vec3 rotation)
	{
		// @todo
		updateViewMatrix();
	};

	void rotate(float yaw, float pitch)
	{
		this->yaw += yaw;
		this->pitch += pitch;
		updateViewMatrix();
	}

	void rotate(glm::vec3 delta)
	{
		// @todo
		updateViewMatrix();
	}

	void setTranslation(glm::vec3 translation)
	{
		this->position = translation;
		updateViewMatrix();
	};

	void translate(glm::vec3 delta)
	{
		this->position += delta;
		updateViewMatrix();
	}

	void update(float deltaTime)
	{
		updated = false;
		if (type == CameraType::firstperson)
		{
			if (moving())
			{
				if (currMovementSpeed < movementSpeed) {
					currMovementSpeed = movementSpeed;
				}
				if (keys.shift) {
					currMovementSpeed += deltaTime * 10.0f;
					if (currMovementSpeed > movementSpeed * 2.5f) {
						currMovementSpeed -= movementSpeed * 2.5f;
					}
					//std::cout << currMovementSpeed << "\n";
					currMovementSpeed = 150.0f;
				} else {
					currMovementSpeed = movementSpeed;
				}

				float moveSpeed = deltaTime * currMovementSpeed;
				glm::vec3 front = frontVector();
				if (keys.up)
					position += front * moveSpeed;
				if (keys.down)
					position -= front * moveSpeed;
				if (keys.left)
					position -= glm::normalize(glm::cross(front, upVector)) * moveSpeed;
				if (keys.right)
					position += glm::normalize(glm::cross(front, upVector)) * moveSpeed;
				updateViewMatrix();
			}
		}
	};

	// Update camera passing separate axis data (gamepad)
	// Returns true if view or position has been changed
	bool updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime)
	{
		bool retVal = false;

		if (type == CameraType::firstperson)
		{
			// Use the common console thumbstick layout		
			// Left = view, right = move

			const float deadZone = 0.0015f;
			const float range = 1.0f - deadZone;

			glm::vec3 camFront = frontVector();

			float moveSpeed = deltaTime * movementSpeed * 2.0f;
			float rotSpeed = deltaTime * rotationSpeed * 50.0f;
			 
			// Move
			if (fabsf(axisLeft.y) > deadZone)
			{
				float pos = (fabsf(axisLeft.y) - deadZone) / range;
				position -= camFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
				retVal = true;
			}
			if (fabsf(axisLeft.x) > deadZone)
			{
				float pos = (fabsf(axisLeft.x) - deadZone) / range;
				position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
				retVal = true;
			}

			// Rotate
			if (fabsf(axisRight.x) > deadZone)
			{
				float pos = (fabsf(axisRight.x) - deadZone) / range;
				//rotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
				retVal = true;
			}
			if (fabsf(axisRight.y) > deadZone)
			{
				float pos = (fabsf(axisRight.y) - deadZone) / range;
				//rotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
				retVal = true;
			}
		}
		else
		{
			// todo: move code from example base class for look-at
		}

		if (retVal)
		{
			updateViewMatrix();
		}

		return retVal;
	}

};