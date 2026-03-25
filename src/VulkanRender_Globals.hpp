#pragma once

#include "camera.hpp"

/**
 * @file VulkanRender_Globals.hpp
 * @brief 跨编译单元共享的全局状态（相机与首帧鼠标），供 GLFW 回调与 VulkanRender 共用。
 *
 * @details 教学说明：工程里把 `camera` 做成全局是为了回调写法简单；更规范的做法是把
 * `VulkanRender*` 放进 `glfwSetWindowUserPointer`，所有状态挂在应用对象上，避免单例式全局。
 */

/** @brief 全局相机：主循环与 uniform 更新、ImGuizmo 共用同一套 view */
extern Camera camera;

/** @brief 首次进入鼠标回调时为 true，用于避免第一帧产生巨大 delta 跳变 */
extern bool firstMouse;

/** @brief 鼠标右键是否按下；为 true 时移动鼠标才旋转相机 */
extern bool rightMouseStatus;

/** @brief 上一帧鼠标 X（窗口像素坐标），用于计算 deltaX */
extern float lastX;

/** @brief 上一帧鼠标 Y（窗口像素坐标），用于计算 deltaY */
extern float lastY;
