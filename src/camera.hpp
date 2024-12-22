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

// �������
enum Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};




class Camera
{
public:
	//���湹��
	Camera(glm::vec3 position, glm::vec3 target, glm::vec3 worldup);
	//ŷ���ǹ���
	Camera(glm::vec3 position, float pitch, float yaw, glm::vec3 worldup);

	glm::vec3 Position;//�����λ��
	glm::vec3 Forward;//���������
	glm::vec3 Right;//����
	glm::vec3 Up;//����
	glm::vec3 WorldUp;//��������
	float Pitch;//ŷ��_������
	float Yaw;//ŷ��_ƫ����
	float SenceX = 0.001f;//����ƶ�X����
	float SenceY = 0.001f;//����ƶ�Y����
	float speedX = 0.0f;//����X�ƶ�����
	float speedY = 0.0f;//����Y�ƶ�����
	float speedZ = 0.0f;//����Z�ƶ�����
	//��ȡ�۲����
	glm::mat4 GetViewMatrix();
	//����ƶ�
	void ProcessMouseMovement(float deltaX, float deltaY);
	//���������λ��
	void UpdataCameraPosition();
	void SetSpeed(float speed);

	// Ĭ������
	float SPEED = 0.1;
private:
	//����������Ƕ�
	void UpdataCameraVectors();
};
#endif