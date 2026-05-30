#pragma once
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

/**
 * @brief Per-object TRS (Translation-Rotation-Scale) transform.
 *
 * Provides GetModelMatrix() and GetNormalMatrix() computed from the TRS decomposition.
 * Default state is identity (no translation, identity quaternion, uniform scale 1).
 */
struct ObjectTransform {
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); ///< Identity quaternion
    glm::vec3 scale    = { 1.0f, 1.0f, 1.0f };

    /**
     * @brief Returns the model matrix: T * R * S.
     *
     * Column-major, suitable for direct upload to Vulkan push constants / UBO.
     */
    glm::mat4 GetModelMatrix() const
    {
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
        const glm::mat4 R = glm::mat4_cast(rotation);
        const glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
        return T * R * S;
    }

    /**
     * @brief Returns the normal matrix: mat4(transpose(inverse(mat3(model)))).
     *
     * Handles non-uniform scale correctly.  For uniform scale, inverse == 1/s * I,
     * so the transpose-inverse reduces to the rotation matrix, but this general form
     * works in all cases.
     *
     * @note This should be computed once per draw call on the CPU and uploaded as a
     *       push constant, NOT recomputed per-vertex in the shader.
     */
    glm::mat4 GetNormalMatrix() const
    {
        return glm::mat4(glm::transpose(glm::inverse(glm::mat3(GetModelMatrix()))));
    }
};
