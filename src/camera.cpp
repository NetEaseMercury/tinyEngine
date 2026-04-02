#include "camera.hpp"

/**
 * @brief Construct camera from position, look-at target, and world up direction.
 *        Derives Forward from (target - position), then cross-products for Right and Up.
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
 * @brief Construct camera from position and Euler angles (radians).
 *        Computes Forward via spherical-coordinate formula from Pitch/Yaw.
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
 * @brief Return the view matrix: lookAt(Position, Position + Forward, WorldUp).
 *        Combine with projection as proj * view * model.
 */
glm::mat4 Camera::GetViewMatrix()
{
	return glm::lookAt(Position, Position + Forward, WorldUp);
}

/**
 * @brief Apply mouse drag delta to Pitch and Yaw.
 *        Pitch is clamped to [-89, 89] degrees to prevent gimbal-lock flip.
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
 * @brief Recompute Forward, Right, Up basis vectors from current Pitch/Yaw
 *        using the spherical-coordinate to Cartesian conversion.
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
 * @brief Integrate Position along Forward/Right/Up basis scaled by speed inputs and SPEED.
 *        speedX/speedY/speedZ are set externally by keyboard callbacks.
 */
void Camera::UpdataCameraPosition()
{
	Position += Forward * speedZ * SPEED + Right * speedX * SPEED + Up * speedY * SPEED;
}

/** @brief Set the per-frame movement multiplier SPEED. */
void Camera::SetSpeed(float speed)
{
	SPEED = speed;
}

/**
 * @brief Begin a smooth-focus animation: fly the camera toward a world point while looking at it.
 * @param worldFocusPoint  World-space point to look at (e.g. bounding-box center of a model).
 * @param cameraDistance   Desired distance from the focus point at animation end.
 * @param durationSec      Animation duration in seconds; values <= 0.01 fall back to 0.65s.
 *
 * The end position is placed along the current view direction (away from the focus point)
 * to preserve the user's viewing angle. If the camera is nearly coincident with the focus
 * point, a fixed fallback offset is used to avoid a zero-length direction vector.
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

/** @brief Hermite smoothstep: cubic ease-in-out curve mapping [0,1] -> [0,1]. */
static float smoothstep01(float x)
{
	x = glm::clamp(x, 0.0f, 1.0f);
	return x * x * (3.0f - 2.0f * x);
}

/**
 * @brief Advance the smooth-focus animation by deltaTime seconds.
 *        Position is interpolated via smoothstep between start and end points;
 *        Pitch/Yaw are recalculated each frame to keep the camera aimed at the focus target.
 *        Automatically deactivates when t >= 1.
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
