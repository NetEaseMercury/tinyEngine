
#include "mainWindows.h"
#include "VulkanRender.hpp"

using namespace std;

/**
 * @brief 程序入口：构造 VulkanRender，调用 Run() 进入 GLFW + Vulkan 主循环。
 * @return 成功为 0；捕获到 std::exception 时打印信息后返回 -1
 */
int main(void)
{
	VulkanRender vr;
	try {
		vr.Run();
	}
	catch (const std::exception e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}
