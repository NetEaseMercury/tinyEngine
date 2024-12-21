#ifndef VULKANRENDERBASE_
#define VULKANRENDERBASE_

#include <string>
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
	virtual void loadModel(std::string modelPath) = 0;
	
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
