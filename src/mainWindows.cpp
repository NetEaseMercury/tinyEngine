
#include "mainWindows.h"
#include "Application.hpp"

using namespace std;

/**
 * @brief 程序入口：构造 Application，调用 run() 进入 GLFW + Vulkan 主循环。
 * @return 成功为 0；捕获到 std::exception 时打印信息后返回 -1
 */
int main(void)
{
	Application app;
	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}
