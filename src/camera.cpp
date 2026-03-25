#include "camera.hpp"

/**
 * @brief ????????lookAt ????? position??target ??? Forward?????? WorldUp ?? Right??Up??
 */
Camera::Camera(glm::vec3 position, glm::vec3 target, glm::vec3 worldup)
{
	Position = position;
	WorldUp = worldup;
	Forward = glm::normalize(target - position);
	Right = glm::normalize(glm::cross(Forward, WorldUp));
	Up = glm::normalize(glm::cross(Right, Forward));
}

/**
 * @brief ????????????????? cos/sin ????? Pitch??Yaw §Õ?? Forward?????? Right??Up??
 */
Camera::Camera(glm::vec3 position, float pitch, float yaw, glm::vec3 worldup)
{
	Position = position;
	WorldUp = worldup;
	Pitch = pitch;
	Yaw = yaw;
	Forward.x = glm::cos(Pitch) * glm::sin(Yaw);
	Forward.y = glm::sin(Pitch);
	Forward.z = glm::cos(Pitch) * glm::cos(Yaw);
	Right = glm::normalize(glm::cross(Forward, WorldUp));
	Up = glm::normalize(glm::cross(Right, Forward));
}

/**
 * @brief ?????????????????? glm????????? proj * view * model??
 */
glm::mat4 Camera::GetViewMatrix()
{
	return glm::lookAt(Position, Position + Forward, WorldUp);
}

/**
 * @brief ?? camera.hpp ????????????????????Pitch ???? ??89?? ?????????????
 */
void Camera::ProcessMouseMovement(float deltaX, float deltaY)
{
	Pitch -= deltaY * SenceX;
	Yaw += deltaX * SenceY;
	if (Pitch > 89.0f)
		Pitch = 89.0f;
	if (Pitch < -89.0f)
		Pitch = -89.0f;
	UpdataCameraVectors();
}

/**
 * @brief ?? camera.hpp ?????????? Pitch/Yaw ??? Forward??Right??Up??
 */
void Camera::UpdataCameraVectors()
{
	Forward.x = glm::cos(Pitch) * glm::sin(Yaw);
	Forward.y = glm::sin(Pitch);
	Forward.z = glm::cos(Pitch) * glm::cos(Yaw);
	Right = glm::normalize(glm::cross(Forward, WorldUp));
	Up = glm::normalize(glm::cross(Right, Forward));
}

/**
 * @brief ?? camera.hpp ????????????????????????? Position??
 */
void Camera::UpdataCameraPosition()
{
	Position += Forward * speedZ * SPEED + Right * speedX * SPEED + Up * speedY * SPEED;
}

/** @brief ???§Õ?? SPEED ??? */
void Camera::SetSpeed(float speed)
{
	SPEED = speed;
}

/**
 * @brief ?? camera.hpp ???????????????????????????????????
 */
void Camera::BeginSmoothFocus(const glm::vec3& worldFocusPoint, float cameraDistance, float durationSec)
{
	smoothFocusTarget_ = worldFocusPoint;
	smoothFocusActive_ = true;
	smoothFocusT_ = 0.f;
	smoothFocusDuration_ = durationSec > 0.01f ? durationSec : 0.65f;
	smoothFocusPos0_ = Position;
	glm::vec3 w = Position - worldFocusPoint;
	if (glm::length(w) < 1e-3f) {
		w = glm::vec3(0.35f, 0.4f, 1.0f);
	}
	w = glm::normalize(w);
	const float dist = glm::max(0.05f, cameraDistance);
	smoothFocusPos1_ = worldFocusPoint + w * dist;
}

static float smoothstep01(float x)
{
	x = glm::clamp(x, 0.0f, 1.0f);
	return x * x * (3.0f - 2.0f * x);
}

/**
 * @brief ?? camera.hpp ???????????¦Ë?¨°????????????? smoothFocusTarget_??
 */
void Camera::UpdateSmoothFocus(float deltaTime)
{
	if (!smoothFocusActive_) {
		return;
	}
	smoothFocusT_ += deltaTime / smoothFocusDuration_;
	const float u = smoothstep01(smoothFocusT_);
	Position = glm::mix(smoothFocusPos0_, smoothFocusPos1_, u);
	glm::vec3 toFocus = smoothFocusTarget_ - Position;
	const float len = glm::length(toFocus);
	if (len > 1e-5f) {
		glm::vec3 fwd = toFocus / len;
		Pitch = glm::asin(glm::clamp(fwd.y, -1.0f, 1.0f));
		Yaw = glm::atan(fwd.x, fwd.z);
	}
	UpdataCameraVectors();
	if (smoothFocusT_ >= 1.0f) {
		smoothFocusActive_ = false;
	}
}
