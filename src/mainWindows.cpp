
#include "mainWindows.h"
#include "VulkanRender.hpp"

using namespace std;

int main(void)
{
    VulkanRender vr;
    try {
        vr.Run();
    }
    catch(const std::exception e){
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}