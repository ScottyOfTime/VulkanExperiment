#ifndef _CAMERA_H
#define _CAMERA_H

#include "vk_types.h"

struct Camera {
	glm::vec3 position = { 0.0f, 0.0f, 0.0f };
	glm::quat orientation = { 1.0f, 0.0f, 0.0f, 0.0f };

	glm::mat4 calcViewMat() {
		glm::mat4 translation = glm::translate(glm::mat4(1.f), position);
		glm::mat4 rotation = glm::toMat4(orientation);
		return glm::inverse(translation * rotation);
	}

	glm::vec3 calcFrontVec() {
		return orientation * glm::vec3(0.0f, 0.0f, -1.0f);
	}

	void rotate(float pitch, float yaw) {
		orientation = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
			orientation;
		orientation *= glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
	}

	void move(glm::vec3 vec) {
		position += glm::vec3(glm::toMat4(orientation) *
			glm::vec4(vec * 0.5f, 0.f));
	}

	void lookAt(glm::vec3 target) {
		glm::vec3 dir = glm::normalize(target - position);
		orientation = glm::quatLookAt(dir, glm::vec3(0.0f, 1.0f, 0.0f));
	}
};

#endif /* _CAMERA_H */
