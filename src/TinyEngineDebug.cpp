/**
 * @file TinyEngineDebug.cpp
 * @brief VK_EXT_debug_utils function pointer loading + RenderDoc runtime API init.
 */
#include "TinyEngineDebug.hpp"
#include <cctype>
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

void preInstanceRenderDocSetup()
{
#ifdef _WIN32
    // Enable RenderDoc's Vulkan capture layer for this process. Must be set
    // before the Vulkan loader enumerates layers or the layer stays inactive.
    SetEnvironmentVariableA("ENABLE_VULKAN_RENDERDOC_CAPTURE", "1");

    // If the system has no RenderDoc registered as an implicit layer, fall back
    // to the copy shipped next to the engine DLL (renderdoc.dll + renderdoc.json).
    // VK_ADD_IMPLICIT_LAYER_PATH makes the loader scan that directory for layer
    // manifests in addition to the registered ones (Vulkan loader 1.3.234+).
    HKEY key{};
    bool systemLayerRegistered = false;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        char  valueName[MAX_PATH];
        DWORD idx = 0, nameLen = MAX_PATH;
        while (RegEnumValueA(key, idx, valueName, &nameLen, nullptr, nullptr, nullptr, nullptr)
               == ERROR_SUCCESS) {
            std::string v(valueName);
            for (auto& c : v) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (v.find("renderdoc") != std::string::npos) { systemLayerRegistered = true; break; }
            ++idx; nameLen = MAX_PATH;
        }
        RegCloseKey(key);
    }

    if (!systemLayerRegistered) {
        // Locate the directory of this engine module; the bundled layer sits there.
        HMODULE self{};
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&preInstanceRenderDocSetup), &self);
        char selfPath[MAX_PATH] = {};
        GetModuleFileNameA(self, selfPath, MAX_PATH);
        std::string dir(selfPath);
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir.resize(slash);
        SetEnvironmentVariableA("VK_ADD_IMPLICIT_LAYER_PATH", dir.c_str());
        rdocStatus = "RenderDoc: no system layer registered; using bundled layer in " + dir;
    }
#endif
}

void initRenderDoc()
{
#ifdef _WIN32
    // Idempotent safeguard in case pre-instance setup was skipped.
    SetEnvironmentVariableA("ENABLE_VULKAN_RENDERDOC_CAPTURE", "1");

    // We MUST obtain the API from the same renderdoc.dll that the Vulkan loader
    // picked up as the implicit layer (typically the system-installed one). If
    // we also LoadLibrary'd our bundled dll, two independent renderdoc.dll
    // instances would coexist: layer hooks would run in one, our TriggerCapture
    // would flip a flag in the other, and no capture would ever happen. So we
    // ONLY pick up an already-loaded renderdoc.dll here.
    HMODULE mod = GetModuleHandleA("renderdoc.dll");
    if (!mod) {
        rdocStatus = "RenderDoc: renderdoc.dll not loaded (Vulkan capture layer "
                     "was not picked up by the loader — ENABLE_VULKAN_RENDERDOC_CAPTURE "
                     "must be set before the process starts).";
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
        // Report which renderdoc.dll we bound to, so double-DLL situations are
        // visible in the log.
        char dllPath[MAX_PATH] = {};
        GetModuleFileNameA(mod, dllPath, MAX_PATH);
        appendRenderDocStatus(("RenderDoc API " + std::to_string(major) + "." + std::to_string(minor) +
                     "." + std::to_string(patch) + " bound to " + dllPath +
                     "; captures -> " + tmpl + "_frameN.rdc").c_str());
        std::cout << "[TinyEngine] " << rdocStatus << "\n";
    } else {
        rdocApi = nullptr;
        appendRenderDocStatus("RenderDoc: RENDERDOC_GetAPI returned failure (dll too old for requested API).");
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

void appendRenderDocStatus(const char* line)
{
    if (!line) return;
    if (!rdocStatus.empty()) rdocStatus += "\n";
    rdocStatus += line;
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

int getLastCapturePath(char* outBuf, int outBufSize)
{
    if (!rdocApi || !outBuf || outBufSize <= 0) return -1;
    outBuf[0] = '\0';
    const uint32_t n = rdocApi->GetNumCaptures();
    if (n == 0) return 0;
    uint32_t pathLen = 0;
    uint64_t timestamp = 0;
    // First call: query the required path length (pathLen includes NUL).
    if (!rdocApi->GetCapture(n - 1, nullptr, &pathLen, &timestamp)) return -1;
    if (pathLen == 0) return 0;
    if (static_cast<int>(pathLen) > outBufSize) return -1; // caller buffer too small
    if (!rdocApi->GetCapture(n - 1, outBuf, nullptr, &timestamp)) return -1;
    return static_cast<int>(pathLen > 0 ? pathLen - 1 : 0);
}

const char* getCaptureFilePathTemplate()
{
    if (!rdocApi) return nullptr;
    return rdocApi->GetCaptureFilePathTemplate();
}

bool launchReplayUI()
{
    if (!rdocApi) return false;
    // Already connected to a running UI (e.g. launched from RenderDoc)? Nothing to do.
    if (rdocApi->IsTargetControlConnected()) return true;
    // 1 = connect the newly launched UI back to this process via target control,
    // so captures show up in its capture list automatically.
    return rdocApi->LaunchReplayUI(1, nullptr) != 0;
}

} // namespace tinyengine::debug
