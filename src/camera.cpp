#include "camera.hpp"

//���湹��
Camera::Camera(glm::vec3 position, glm::vec3 target, glm::vec3 worldup)
{
	Position = position;
	WorldUp = worldup;
	Forward = glm::normalize(target - position);
	Right = glm::normalize(glm::cross(Forward, WorldUp));
	Up = glm::normalize(glm::cross(Right, Forward));
}

//ŷ���ǹ���
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

//��ȡ��ͼ�۲����
glm::mat4 Camera::GetViewMatrix()
{
	return glm::lookAt(Position, Position + Forward, WorldUp);
}

//����������Ƕ�
void Camera::UpdataCameraVectors()
{
	Forward.x = glm::cos(Pitch) * glm::sin(Yaw);
	Forward.y = glm::sin(Pitch);
	Forward.z = glm::cos(Pitch) * glm::cos(Yaw);
	Right = glm::normalize(glm::cross(Forward, WorldUp));
	Up = glm::normalize(glm::cross(Right, Forward));
}

//���������λ��
void Camera::UpdataCameraPosition()
{
	//Position += glm::vec3(speedX, speedY,-speedZ) * 0.3f;
	Position += Forward * speedZ * 0.001f + Right * speedX * 0.001f + Up * speedY * 0.001f;
}