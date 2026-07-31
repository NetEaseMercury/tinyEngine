#ifndef CAMERA_H
#define CAMERA_H
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif // !GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/quaternion.hpp>

/**
 * @brief WASD movement-direction enum; extendable by upper-layer callers.
 */
enum Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};

/**
 * @class Camera
 * @brief FPS-style free camera; orientation is described by the quaternion
 *        orientation_.
 *
 * @details
 * - Orientation is driven by two independent accumulators:
 *     yawAccum_   — yaw around the world WorldUp axis (unclamped)
 *     pitchAccum_ — pitch around the local X axis (clamped to ±89°)
 *   Composition: orientation_ = yawQuat * pitchQuat
 * - Forward / Right / Up are derived by rotating basis vectors with
 *   orientation_, updated every frame.
 * - GetViewMatrix() uses glm::lookAt(Position, Position+Forward, WorldUp).
 * - UpdataCameraPosition(dt) takes deltaTime for frame-rate-independent speed.
 * - SmoothFocus smoothly interpolates the position and writes back into
 *   pitchAccum_/yawAccum_ without disturbing the quaternion logic.
 */
class Camera
{
public:
	/**
	 * @brief Construct from "position + look-at target + world up".
	 * @param position Camera world position
	 * @param target   Look-at point (world coordinates)
	 * @param worldup  World up direction, usually (0,1,0)
	 */
	Camera(glm::vec3 position, glm::vec3 target, glm::vec3 worldup);

	/**
	 * @brief Construct from "position + pitch/yaw (radians) + world up".
	 * @param position Camera world position
	 * @param pitch    Pitch angle (radians); positive looks up, clamped to ±89°
	 * @param yaw      Yaw angle (radians), around WorldUp
	 * @param worldup  World up direction
	 */
	Camera(glm::vec3 position, float pitch, float yaw, glm::vec3 worldup);

	/** @brief Camera world position */
	glm::vec3 Position;
	/** @brief Unit forward vector, derived from orientation_, updated per frame */
	glm::vec3 Forward;
	/** @brief Unit right vector */
	glm::vec3 Right;
	/** @brief Unit up vector (camera-local) */
	glm::vec3 Up;
	/** @brief World up reference, usually (0,1,0) */
	glm::vec3 WorldUp;

	/** @brief Pitch sensitivity (radians/pixel), affects mouse Y axis */
	float SensitivityPitch = 0.003f;
	/** @brief Yaw sensitivity (radians/pixel), affects mouse X axis */
	float SensitivityYaw   = 0.003f;

	/** @brief Normalized speed input along Right [-1,1], driven by A/D keys */
	float speedX = 0.0f;
	/** @brief Normalized speed input along Up (reserved for Q/E, default 0) */
	float speedY = 0.0f;
	/** @brief Normalized speed input along Forward [-1,1], driven by W/S keys */
	float speedZ = 0.0f;

	/** @brief Base movement speed (units/second), set from the UI slider */
	float SPEED = 5.0f;

	// ── Projection parameters ──────────────────────────────────────────────────
	/** @brief Field of view (degrees), default 45° */
	float FovDeg     = 45.0f;
	/** @brief Near clip plane distance */
	float NearPlane  = 0.1f;
	/** @brief Far clip plane distance */
	float FarPlane   = 500.0f;
	/** @brief Aspect ratio (width/height), set via SetAspectRatio */
	float AspectRatio = 1.0f;

	/**
	 * @brief Update the aspect ratio when the swapchain size changes.
	 * @param width  Framebuffer width (pixels)
	 * @param height Framebuffer height (pixels)
	 */
	void SetAspectRatio(float width, float height);

/**
 * @brief Return the View matrix derived from worldTransform_ (no glm::lookAt,
 *        O(1) transposed rotation).
 */
glm::mat4 GetViewMatrix() const;

/**
 * @brief Return the Projection matrix with Vulkan Y-flip (for normal rendering).
 */
glm::mat4 GetProjectionMatrix() const;

/**
 * @brief Return the Projection matrix without Y-flip (for OpenGL-convention
 *        code such as ImGuizmo).
 */
glm::mat4 GetProjectionMatrixNoFlip() const;

/**
 * @brief Return premultiplied proj * view (ready for UBO/shader use, saves one
 *        matrix multiply).
 */
glm::mat4 GetViewProjectionMatrix() const;

/**
 * @brief Return the camera world position, taken directly from
 *        worldTransform_[3], replacing an external glm::inverse(view)[3].
 */
glm::vec3 GetWorldPosition() const;

/**
 * @brief Return the camera's Camera-to-World matrix for external systems that
 *        need the camera pose.
 */
glm::mat4 GetWorldTransform() const;

	/**
	 * @brief FPS-style rotation: right-mouse-drag turns the camera in place
	 *        (yaw/pitch). Mouse right → yaw decreases → camera turns right →
	 *        the scene shifts left.
	 * @param deltaX X pixel displacement since last frame (positive = right)
	 * @param deltaY Y pixel displacement since last frame (positive = down)
	 */
	void ProcessMouseMovement(float deltaX, float deltaY);

	/**
	 * @brief Set the orbit-rotation center (world coordinates), usually the model
	 *        center. BeginSmoothFocus calls this automatically; also call it once
	 *        manually after loading a model.
	 */
	void SetOrbitCenter(const glm::vec3& center) { orbitCenter_ = center; }

	/**
	 * @brief Update Position from speedX/Y/Z, SPEED and deltaTime.
	 * @param deltaTime Frame duration (seconds); keeps movement frame-rate independent
	 */
	void UpdataCameraPosition(float deltaTime);

	/** @brief Set the base movement speed; usually called from a UI slider */
	void SetSpeed(float speed);

	/**
	 * @brief Begin a smooth-focus animation that moves the camera near
	 *        worldFocusPoint and looks at it.
	 * @param worldFocusPoint Target focus point (world coordinates)
	 * @param cameraDistance  Camera distance from the target when the animation ends
	 * @param durationSec     Animation duration (seconds); <0.01 uses the default 0.65s
	 */
	void BeginSmoothFocus(const glm::vec3& worldFocusPoint, float cameraDistance, float durationSec);

	/**
	 * @brief Advance the smooth focus per frame; call every frame with deltaTime.
	 */
	void UpdateSmoothFocus(float deltaTime);

	/** @brief True while a smooth focus is running; WASD movement should be disabled during it */
	[[nodiscard]] bool IsSmoothFocusActive() const { return smoothFocusActive_; }

private:
	/** @brief Camera orientation quaternion, composed from yawAccum_ and pitchAccum_ */
	glm::quat orientation_;

	/** @brief Accumulated yaw (radians) around WorldUp, unclamped */
	float yawAccum_   = 0.0f;
	/** @brief Accumulated pitch (radians), clamped to [-89°, +89°] */
	float pitchAccum_ = 0.0f;

	/** @brief Camera-to-World matrix; maintained by UpdataCameraVectors(), always in sync with Position/orientation_ */
	glm::mat4 worldTransform_{ 1.0f };

	/** @brief Rebuild Forward / Right / Up from orientation_, also rebuilding worldTransform_ */
	void UpdataCameraVectors();

	/** @brief Rebuild orientation_ from pitchAccum_ / yawAccum_, then call UpdataCameraVectors */
	void RebuildOrientation();

	bool      smoothFocusActive_   = false;
	float     smoothFocusT_        = 0.f;
	float     smoothFocusDuration_ = 0.65f;
	glm::vec3 smoothFocusPos0_{};
	glm::vec3 smoothFocusPos1_{};
	glm::vec3 smoothFocusTarget_{};

	/** @brief Orbit-rotation center (world coordinates); right-drag orbits the camera around this point */
	glm::vec3 orbitCenter_{ 0.0f, 0.0f, 0.0f };
};
#endif
