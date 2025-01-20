
#include "mainWindows.h"
#include "engine/Application.h"
using namespace std;
void printUsage() {
    std::cout << "Usage:" << std::endl
        << "\t" << "LeoEngine.exe [my_file.scene]" << "\t" << "Open the scene file with the renderer." << std::endl
        << "\t" << "LeoEngine.exe --help [...]" << "\t" << "Print this help." << std::endl;
    std::cout << "Notes:" << std::endl
        << "\t" << "If no scene file is provided, will open \"resources/models/Sponza/super_sponza.scene\"." << std::endl << std::endl;
}

int main(int argc, const char** argv)
{

    const char* scenePath = "res/models/Sponza/super_sponza.scene";
    if (argc == 2) {
        if (!strcmp(argv[1], "--help")) {
            printUsage();
            return 0;
        }
        else {
            scenePath = argv[1];
        }
    }

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