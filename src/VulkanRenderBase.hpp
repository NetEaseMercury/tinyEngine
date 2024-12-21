#ifndef VULKANRENDERBASE_
#define VULKANRENDERBASE_

#include <string>
/// <summary>
/// vulkan引擎基类
/// </summary>
class VulkanRenderBase {
public:
	virtual ~VulkanRenderBase() {}
	/// <summary>
	/// 引擎开始运行逻辑，包含变量参数初始化等
	/// </summary>
	virtual void Run() = 0;

	/// <summary>
	/// 引擎推出逻辑，包含状态清理等
	/// </summary>
	virtual void Escape() = 0;

	/// <summary>
	/// 模型加载，用于获取模型的顶点信息
	/// </summary>
	/// <param name="modelPath"></param>
	virtual void loadModel(std::string modelPath) = 0;
	
	/// <summary>
	/// 引擎主循环函数
	/// </summary>
	virtual void gameLoop() = 0;

	/// <summary>
	/// 引擎初始化函数，用于Vulkan引擎及其他库初始化
	/// </summary>
	virtual void initEngine() = 0;
};

#endif // !VULKANRENDERBASE_
