#pragma once

#include "EngineRuntime.hpp"
#include "Renderer.hpp"
#include "tinyengine_api.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <variant>

/**
 * @class Presenter
 * @brief Present layer (C++ side): assembles Runtime + Renderer into a render
 *        thread and serves the C ABI in a "command queue in / snapshot out"
 *        style.
 *
 * Threading model:
 *  - start() launches the render thread and synchronously waits for the Vulkan
 *    init result;
 *  - all cmd* methods are thread-safe (callable from any thread); commands are
 *    drained and applied at the start of each render-thread frame;
 *  - getSnapshot() is thread-safe; it copies the read-only snapshot published
 *    at the end of each render-thread frame;
 *  - window creation / GLFW events / the Vulkan frame loop all live on the
 *    render thread.
 */
class Presenter {
public:
    using LogSink = std::function<void(int level, const std::string& msg)>;

    Presenter();
    ~Presenter();

    /** @brief Launch the render thread and block until init completes; returns true on success, fills outError on failure. */
    bool start(const std::string& resRoot, std::string* outError = nullptr);
    /** @brief Stop the render thread and wait for it to exit (idempotent). */
    void stop();

    void setLogSink(LogSink sink) { logSink_ = std::move(sink); }

    // ── Viewport (command-style, executed on the render thread) ─────────────
    void viewportAttach(void* hwndParent, int w, int h);
    void viewportResize(int w, int h);

    // ── Scene commands (thread-safe) ───────────────────────────────────────
    void     cmdLoadModel(const std::string& relPath);
    void     cmdLoadMaterialAst(const std::string& relPath);
    uint64_t cmdAddBox(float x, float y, float z);
    void     cmdRemoveBox(uint64_t id);
    void     cmdSetMaterialParams(uint32_t id, const float* baseColor4,
                                  float roughness, float metallic,
                                  float emissiveIntensity, const float* emissiveColor4);
    void     cmdSetClearColor(float r, float g, float b, float a);
    void     cmdSelect(int what, uint64_t boxId);
    void     cmdAssignMaterial(int what, uint64_t boxId, uint32_t materialId);
    void     cmdSetModelPosition(float x, float y, float z);
    void     cmdSetBoxPosition(uint64_t id, float x, float y, float z);

    /** @brief Copy the latest snapshot; zero-filled if the engine has not published one yet. */
    void getSnapshot(TeSceneSnapshot& out) const;

private:
    // ── Command definitions ─────────────────────────────────────────────────
    struct CmdAttach      { void* hwnd; int w, h; };
    struct CmdResize      { int w, h; };
    struct CmdLoadModel   { std::string path; };
    struct CmdLoadAst     { std::string path; };
    struct CmdAddBox      { uint64_t id; float x, y, z; };
    struct CmdRemoveBox   { uint64_t id; };
    struct CmdSetMatParams{ uint32_t id; float baseColor[4]; float roughness, metallic, emissiveIntensity; float emissiveColor[4]; };
    struct CmdClearColor  { float r, g, b, a; };
    struct CmdSelect      { int what; uint64_t boxId; };
    struct CmdAssignMat   { int what; uint64_t boxId; uint32_t materialId; };
    struct CmdModelPos    { float x, y, z; };
    struct CmdBoxPos      { uint64_t id; float x, y, z; };
    using Command = std::variant<CmdAttach, CmdResize, CmdLoadModel, CmdLoadAst,
                                 CmdAddBox, CmdRemoveBox, CmdSetMatParams, CmdClearColor,
                                 CmdSelect, CmdAssignMat, CmdModelPos, CmdBoxPos>;

    void enqueue(Command cmd);

    // ── Render thread ──────────────────────────────────────────────────────
    void threadMain();
    void threadInit();          // GLFW + Vulkan + initial scene (throwing = failure)
    void drainCommands();
    void applyCommand(const Command& cmd);
    void processInput();
    void publishSnapshot();
    void log(int level, const std::string& msg) const;

    // GLFW static callbacks (userPointer = this)
    static void framebufferResizeCallback(GLFWwindow* w, int, int);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void mouseCallback(GLFWwindow* w, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset);

    // ── Members ────────────────────────────────────────────────────────────
    EngineRuntime runtime_;
    Renderer      renderer_;
    GLFWwindow*   window_ = nullptr;

    std::thread       renderThread_;
    std::atomic<bool> running_{ false };

    // Init synchronization
    std::mutex              initMutex_;
    std::condition_variable initCv_;
    bool        initDone_ = false;
    bool        initOk_   = false;
    std::string initError_;

    // Command queue
    mutable std::mutex    cmdMutex_;
    std::deque<Command>   cmdQueue_;
    std::atomic<uint64_t> nextBoxId_{ 1 };

    // Snapshot
    mutable std::mutex snapMutex_;
    TeSceneSnapshot    snap_{};

    // Input state
    bool  rightMouseDown_ = false;
    bool  firstMouse_     = true;
    float lastX_ = WIDTH / 2.f;
    float lastY_ = HEIGHT / 2.f;

    std::string resRoot_;
    std::string modelPath_;    // Resolved absolute path of the default model
    std::string texturePath_;  // Resolved absolute path of the default texture
    LogSink     logSink_;
};
