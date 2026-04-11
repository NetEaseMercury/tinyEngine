/**
 * @file TinyEngineDebug.cpp
 * @brief VK_EXT_debug_utils function pointer loading + RenderDoc runtime API init.
 */
#include "TinyEngineDebug.hpp"
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace tinyengine::debug {

static PFN_vkCmdBeginDebugUtilsLabelEXT pfnBeginLabel = nullptr;
static PFN_vkCmdEndDebugUtilsLabelEXT   pfnEndLabel   = nullptr;
static RENDERDOC_API_1_6_0*             rdocApi        = nullptr;

void initDebugUtils(VkInstance instance)
{
    pfnBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    pfnEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));

    if (pfnBeginLabel && pfnEndLabel) {
        std::cout << "[TinyEngine] VK_EXT_debug_utils markers enabled.\n";
    }
}

void beginLabel(VkCommandBuffer cmd, const char* name, float r, float g, float b, float a)
{
    if (!pfnBeginLabel) return;
    VkDebugUtilsLabelEXT label{};
    label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0]   = r;
    label.color[1]   = g;
    label.color[2]   = b;
    label.color[3]   = a;
    pfnBeginLabel(cmd, &label);
}

void endLabel(VkCommandBuffer cmd)
{
    if (!pfnEndLabel) return;
    pfnEndLabel(cmd);
}

void initRenderDoc()
{
#ifdef _WIN32
    HMODULE mod = GetModuleHandleA("renderdoc.dll");
    if (!mod) {
        std::cout << "[TinyEngine] RenderDoc not detected (renderdoc.dll not loaded).\n";
        return;
    }
    auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
    if (!getApi) return;

    if (getApi(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&rdocApi)) == 1) {
        int major = 0, minor = 0, patch = 0;
        rdocApi->GetAPIVersion(&major, &minor, &patch);
        std::cout << "[TinyEngine] RenderDoc API " << major << "." << minor << "." << patch
                  << " connected.\n";
    } else {
        rdocApi = nullptr;
    }
#else
    std::cout << "[TinyEngine] RenderDoc in-app API only implemented for Windows.\n";
#endif
}

bool isRenderDocAttached()
{
    return rdocApi != nullptr;
}

RENDERDOC_API_1_6_0* getRenderDocApi()
{
    return rdocApi;
}

} // namespace tinyengine::debug
