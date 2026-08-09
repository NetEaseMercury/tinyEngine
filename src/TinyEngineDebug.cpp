/**
 * @file TinyEngineDebug.cpp
 * @brief VK_EXT_debug_utils function pointer loading + RenderDoc runtime API init.
 */
#include "TinyEngineDebug.hpp"
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace tinyengine::debug {

static PFN_vkCmdBeginDebugUtilsLabelEXT pfnBeginLabel = nullptr;
static PFN_vkCmdEndDebugUtilsLabelEXT   pfnEndLabel   = nullptr;
static RENDERDOC_API_1_6_0*             rdocApi        = nullptr;
static std::string                      rdocStatus;

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
    // RenderDoc's Vulkan capture layer is registered as an implicit layer but is
    // gated behind this env var (see renderdoc.json "enable_environment"). It
    // MUST be set before the Vulkan loader enumerates layers (i.e. before
    // vkCreateInstance) or RenderDoc never hooks the Vulkan command stream and
    // TriggerCapture has no frame to capture. This is the key to self-loaded
    // capture working without launching from the RenderDoc UI.
    SetEnvironmentVariableA("ENABLE_VULKAN_RENDERDOC_CAPTURE", "1");

    // Prefer an already-injected RenderDoc (launched from the RenderDoc UI);
    // otherwise actively load our bundled renderdoc.dll so in-app capture works
    // even when the Editor is started directly. Must happen before
    // vkCreateInstance so RenderDoc can hook the Vulkan loader.
    HMODULE mod = GetModuleHandleA("renderdoc.dll");
    bool alreadyInjected = (mod != nullptr);
    if (!mod) {
        mod = LoadLibraryA("renderdoc.dll");
        if (mod)
            std::cout << "[TinyEngine] renderdoc.dll loaded from application directory.\n";
    }
    if (!mod) {
        rdocStatus = "RenderDoc: renderdoc.dll not found (no injection, LoadLibrary failed).";
        std::cout << "[TinyEngine] " << rdocStatus << "\n";
        return;
    }
    auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
    if (!getApi) {
        rdocStatus = "RenderDoc: RENDERDOC_GetAPI not found in renderdoc.dll.";
        return;
    }

    // Request the newest API we use, falling back to an older one so an older
    // installed renderdoc.dll (which may not implement 1.6.0) still works.
    int ok = getApi(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&rdocApi));
    if (ok != 1)
        ok = getApi(eRENDERDOC_API_Version_1_4_0, reinterpret_cast<void**>(&rdocApi));
    if (ok == 1 && rdocApi) {
        int major = 0, minor = 0, patch = 0;
        rdocApi->GetAPIVersion(&major, &minor, &patch);
        // Save captures under <cwd>/captures/tinyengine_frameN.rdc so they are
        // easy to find (default would be a temp directory).
        char cwd[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        std::string tmpl = std::string(cwd) + "\\captures\\tinyengine";
        CreateDirectoryA((std::string(cwd) + "\\captures").c_str(), nullptr);
        rdocApi->SetCaptureFilePathTemplate(tmpl.c_str());
        rdocStatus = "RenderDoc API " + std::to_string(major) + "." + std::to_string(minor) +
                     "." + std::to_string(patch) + " connected (" +
                     (alreadyInjected ? "UI-injected" : "self-loaded") +
                     "); captures -> " + tmpl + "_frameN.rdc";
        std::cout << "[TinyEngine] " << rdocStatus << "\n";
    } else {
        rdocApi = nullptr;
        rdocStatus = "RenderDoc: RENDERDOC_GetAPI returned failure (dll too old for requested API).";
        std::cout << "[TinyEngine] " << rdocStatus << "\n";
    }
#else
    rdocStatus = "RenderDoc in-app API only implemented for Windows.";
    std::cout << "[TinyEngine] " << rdocStatus << "\n";
#endif
}

const char* getRenderDocStatus()
{
    return rdocStatus.empty() ? "RenderDoc: (not initialized)" : rdocStatus.c_str();
}

bool isRenderDocAttached()
{
    return rdocApi != nullptr;
}

RENDERDOC_API_1_6_0* getRenderDocApi()
{
    return rdocApi;
}

bool triggerCapture()
{
    if (!rdocApi) return false;
    // Requests a capture of the next presented frame. RenderDoc writes the .rdc
    // to the path template set in initRenderDoc (<cwd>/captures/).
    rdocApi->TriggerCapture();
    return true;
}

const char* getCaptureFilePathTemplate()
{
    if (!rdocApi) return nullptr;
    return rdocApi->GetCaptureFilePathTemplate();
}

} // namespace tinyengine::debug
