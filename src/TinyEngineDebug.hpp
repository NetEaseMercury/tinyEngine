#pragma once
/**
 * @file TinyEngineDebug.hpp
 * @brief VK_EXT_debug_utils scoped markers + RenderDoc in-app API integration.
 *
 * Usage:
 *   TINYENGINE(cmd, "My Render Pass")   -- scoped label visible in RenderDoc event browser
 *   tinyengine::debug::isRenderDocAttached()  -- check if RenderDoc is injected
 *   tinyengine::debug::getRenderDocApi()      -- access RENDERDOC_API for capture control
 */

#include <vulkan/vulkan.h>
#include <renderdoc_app.h>

namespace tinyengine::debug {

/// Call once after vkCreateInstance to load VK_EXT_debug_utils function pointers.
void initDebugUtils(VkInstance instance);

/// Insert a debug label begin into the command buffer.
void beginLabel(VkCommandBuffer cmd, const char* name,
                float r = 0.0f, float g = 0.7f, float b = 1.0f, float a = 1.0f);

/// Insert a debug label end into the command buffer.
void endLabel(VkCommandBuffer cmd);

/// Set env vars needed to enable the Vulkan capture layer. Must be called
/// before any Vulkan API is touched (i.e. before vkCreateInstance / even
/// before vkEnumerateInstanceLayerProperties).
void preInstanceRenderDocSetup();

/// Bind the RenderDoc in-app API. Must be called AFTER vkCreateInstance so the
/// Vulkan loader has had a chance to load renderdoc.dll as an implicit layer;
/// otherwise GetModuleHandle("renderdoc.dll") returns null and RenderDoc is
/// considered unavailable this session.
void initRenderDoc();

/// Human-readable status of the last initRenderDoc() call (for surfacing to the
/// Editor log). Never returns nullptr.
const char* getRenderDocStatus();

/// Append a diagnostic line to the RenderDoc status text (surfaced to the log
/// alongside getRenderDocStatus()). Used by other subsystems (e.g. Vulkan
/// instance creation) to report layer enumeration results.
void appendRenderDocStatus(const char* line);

/// True if RenderDoc was detected and the API was successfully loaded.
bool isRenderDocAttached();

/// Access the RenderDoc API for programmatic capture control (may return nullptr).
RENDERDOC_API_1_6_0* getRenderDocApi();

/// Trigger a RenderDoc capture of the next frame. Returns false if RenderDoc is
/// not available. The .rdc capture is saved to RenderDoc's default location.
bool triggerCapture();

/// The capture file path template (e.g. "<cwd>/captures/tinyengine"); RenderDoc
/// appends "_frameN.rdc". Returns nullptr if RenderDoc is not available.
const char* getCaptureFilePathTemplate();

/// Launch the RenderDoc replay UI (qrenderdoc) and connect it to this process
/// via target control, so captures appear in its list automatically. Returns
/// false if RenderDoc is unavailable or the UI could not be launched.
bool launchReplayUI();

} // namespace tinyengine::debug

/// RAII scoped debug marker. Calls beginLabel on construction, endLabel on destruction.
class TinyEngineScopedMarker {
public:
    TinyEngineScopedMarker(VkCommandBuffer cmd, const char* name,
                           float r = 0.0f, float g = 0.7f, float b = 1.0f, float a = 1.0f)
        : cmd_(cmd) {
        tinyengine::debug::beginLabel(cmd, name, r, g, b, a);
    }
    ~TinyEngineScopedMarker() {
        tinyengine::debug::endLabel(cmd_);
    }
    TinyEngineScopedMarker(const TinyEngineScopedMarker&) = delete;
    TinyEngineScopedMarker& operator=(const TinyEngineScopedMarker&) = delete;
private:
    VkCommandBuffer cmd_;
};

#define TINYENGINE_CONCAT_IMPL_(a, b) a##b
#define TINYENGINE_CONCAT_(a, b) TINYENGINE_CONCAT_IMPL_(a, b)

/// Scoped debug marker: visible in RenderDoc / Vulkan debug layers.
/// Example: TINYENGINE(cmd, "Shadow Pass");
#define TINYENGINE(cmd, name) \
    TinyEngineScopedMarker TINYENGINE_CONCAT_(_te_marker_, __LINE__)(cmd, name)
