
#include "mainWindows.h"
#include "engine/Application.h"
using namespace std;

int main(int argc, const char** argv)
{
    // default scene path
    const char* scenePath = "res/models/Sponza/super_sponza.scene";

    GameManage *vr = new GameManage();
    try {
        vr->init();
        vr->loadScene(scenePath);
        vr->start();
        vr->cleanup();
    }
    catch(const std::exception e){
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}