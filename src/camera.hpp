#ifndef CAMERA_H
#define CAMERA_H
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif // !GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define M_PI 3.14159265358979323846

/**
 * @brief 与 WASD 移动语义相关的方向枚举（本项目中相机主要用 speedX/speedZ，该枚举可扩展）
 */
enum Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};

/**
 * @class Camera
 * @brief 第一人称/轨道式相机：用世界空间位置 + 俯仰角 Pitch + 偏航角 Yaw 描述朝向。
 *
 * @details 教学要点：
 * - `GetViewMatrix()` 使用 glm::lookAt：视点为 Position，看向 Position + Forward。
 * - Forward 由 Pitch、Yaw 通过球坐标式公式恢复，与多数 FPS 教程一致。
 * - `UpdataCameraPosition()` 按 Forward / Right / Up 基底做位移，配合 speedX、speedZ 与 SPEED。
 * - 平滑聚焦（SmoothFocus）在每帧用插值移动视点，并始终注视目标点，用于「按 F 对准物体」。
 */
class Camera
{
public:
	/**
	 * @brief 由「位置 + 观察目标 + 世界上方向」构造相机。
	 * @param position 相机所在世界坐标
	 * @param target   视线所指的点（世界坐标）
	 * @param worldup  世界上方向，通常 (0,1,0)，用于确定 Right/Up
	 */
	Camera(glm::vec3 position, glm::vec3 target, glm::vec3 worldup);

	/**
	 * @brief 由位置与欧拉角（弧度）构造相机。
	 * @param position 相机世界坐标
	 * @param pitch    俯仰角（弧度）：抬头为正
	 * @param yaw      偏航角（弧度）：绕世界 Up 旋转
	 * @param worldup  世界上方向
	 */
	Camera(glm::vec3 position, float pitch, float yaw, glm::vec3 worldup);

	/** @brief 相机在世界空间中的位置 */
	glm::vec3 Position;
	/** @brief 归一化前向向量（由 Pitch/Yaw 维护，或由 lookAt 构造推出） */
	glm::vec3 Forward;
	/** @brief 相机右轴（与 Forward、WorldUp 正交） */
	glm::vec3 Right;
	/** @brief 相机上轴 */
	glm::vec3 Up;
	/** @brief 世界上方向参考（通常为 (0,1,0)） */
	glm::vec3 WorldUp;
	/** @brief 俯仰角（弧度） */
	float Pitch;
	/** @brief 偏航角（弧度） */
	float Yaw;
	/** @brief 鼠标影响 Pitch 的灵敏度系数 */
	float SenceX = 0.001f;
	/** @brief 鼠标影响 Yaw 的灵敏度系数 */
	float SenceY = 0.001f;
	/** @brief 沿相机 Right 方向的移动输入 [-1,1]，由键盘 A/D 设置 */
	float speedX = 0.0f;
	/** @brief 沿相机 Up 方向的移动输入（本工程若未绑键则常为 0） */
	float speedY = 0.0f;
	/** @brief 沿相机 Forward 方向的移动输入 [-1,1]，由键盘 W/S 设置 */
	float speedZ = 0.0f;

	/**
	 * @brief 计算观察矩阵，供 Vulkan  uniform / ImGui 场景一致使用。
	 * @return view = lookAt(Position, Position + Forward, WorldUp)
	 */
	glm::mat4 GetViewMatrix();

	/**
	 * @brief 处理鼠标右拖：更新 Pitch/Yaw 并刷新 Forward。
	 * @param deltaX 当前帧相对上一帧的鼠标 X 位移（像素）
	 * @param deltaY 当前帧相对上一帧的鼠标 Y 位移（像素）；下移通常希望俯视，故 Pitch -= deltaY * SenceX
	 */
	void ProcessMouseMovement(float deltaX, float deltaY);

	/**
	 * @brief 根据 speedX/speedY/speedZ 与 SPEED 积分平移 Position（不在此函数内读键盘）
	 */
	void UpdataCameraPosition();

	/** @brief 设置每帧位移倍率（可与 UI 滑动条联动） */
	void SetSpeed(float speed);

	/**
	 * @brief 开始一段平滑「飞到目标旁并注视该点」的动画。
	 * @param worldFocusPoint  要看的世界坐标点（如模型包围盒中心）
	 * @param cameraDistance   动画结束后相机距该点的距离（沿当前视线方向在焦点外侧取点）
	 * @param durationSec      动画时长（秒）；≤0.01 时使用内置默认约 0.65s
	 */
	void BeginSmoothFocus(const glm::vec3& worldFocusPoint, float cameraDistance, float durationSec);

	/**
	 * @brief 推进平滑聚焦动画（应在每帧调用，传入帧间隔 deltaTime）
	 */
	void UpdateSmoothFocus(float deltaTime);

	/** @brief 是否仍处于平滑聚焦中；为真时主循环可不应用 WASD 位移以免冲突 */
	[[nodiscard]] bool IsSmoothFocusActive() const { return smoothFocusActive_; }

	/** @brief 键盘移动速度系数，与 speedX/speedZ 相乘得到实际步长 */
	float SPEED = 0.1f;

private:
	/** @brief 由当前 Pitch、Yaw 重写 Forward、Right、Up */
	void UpdataCameraVectors();

	bool smoothFocusActive_ = false;
	float smoothFocusT_ = 0.f;
	float smoothFocusDuration_ = 0.65f;
	glm::vec3 smoothFocusPos0_{};
	glm::vec3 smoothFocusPos1_{};
	glm::vec3 smoothFocusTarget_{};
};
#endif
