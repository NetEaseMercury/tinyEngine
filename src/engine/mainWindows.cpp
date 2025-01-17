
#include "mainWindows.h"
#include "GameManage.hpp"
using namespace std;

int main(void)
{
    GameManage *vr = new GameManage();
    try {
        vr->Run();
    }
    catch(const std::exception e){
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}