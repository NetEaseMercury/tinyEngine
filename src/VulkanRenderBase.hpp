#ifndef VULKANRENDERBASE_
#define VULKANRENDERBASE_

#include <string>
#include <glm/glm.hpp>
/// <summary>
/// vulkan�������
/// </summary>
class VulkanRenderBase {
public:
	virtual ~VulkanRenderBase() {}
	/// <summary>
	/// ���濪ʼ�����߼�����������������ʼ����
	/// </summary>
	virtual void Run() = 0;

	/// <summary>
	/// �����Ƴ��߼�������״̬�����
	/// </summary>
	virtual void Escape() = 0;

	/// <summary>
	/// ģ�ͼ��أ����ڻ�ȡģ�͵Ķ�����Ϣ
	/// </summary>
	/// <param name="modelPath"></param>
	virtual void loadModel(std::string modelPath, glm::vec3 position) = 0;
	
	/// <summary>
	/// ������ѭ������
	/// </summary>
	virtual void gameLoop() = 0;

	/// <summary>
	/// �����ʼ������������Vulkan���漰�������ʼ��
	/// </summary>
	virtual void initEngine() = 0;
};

#endif // !VULKANRENDERBASE_
