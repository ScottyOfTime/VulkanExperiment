#ifndef _CAMERA_H
#define _CAMERA_H

#include "vk_types.h"

struct Camera {
	glm::vec3 pos;
	glm::vec3 vel;

	float pitch = 0.f;
	float yaw = 0.f;

	glm::mat4 calcViewMat() {
		glm::mat4 translation = glm::translate(glm::mat4(1.f), pos);
		glm::mat4 rotation = calcRotationMat();
		return glm::inverse(translation * rotation);
	}
	glm::mat4 calcRotationMat() {
		glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
		glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });
		return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
	}
	glm::vec3 calcFrontVec() {
		glm::vec3 front;
		float xzLen = cos(pitch);
		front.x = xzLen * cos(yaw);
		front.y = sin(pitch);
		front.z = xzLen * sin(-yaw);
		return front;
	}
};

#endif /* _CAMERA_H */
