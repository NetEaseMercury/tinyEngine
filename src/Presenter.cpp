#include "Presenter.hpp"
#include "EnginePaths.hpp"
#include "TinyEngineDebug.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

// ── Default asset resolution (same fallback strategy as the old IMGUIManager) ──

std::string resolveModelInDir(const std::string& dirWithSlash, const char* primaryFile)
{
    const char* fallbacks[] = { primaryFile, "01.obj", "no_material.obj" };
    const fs::path dir(dirWithSlash);
    std::error_code ec;
    for (const char* name : fallbacks) {
        const auto f = dir / name;
        if (fs::is_regular_file(f, ec)) return f.string();
    }
    return (dir / primaryFile).string();
}

std::string resolveTextureInDir(const std::string& dirWithSlash, const char* primaryFile)
{
    const char* fallbacks[] = {
        primaryFile, "cyber_room.png", "fantasy_game_inn.png", "viking_room.png", "texture.jpg",
    };
    const fs::path dir(dirWithSlash);
    std::error_code ec;
    for (const char* name : fallbacks) {
        const auto f = dir / name;
        if (fs::is_regular_file(f, ec)) return f.string();
    }
    return (dir / primaryFile).string();
}

// ── Viewport window subclassing: explicitly SetFocus when the child window is
//    clicked, so GLFW receives keyboard input ─────────────────────────────────

WNDPROC g_oldViewportProc = nullptr;

LRESULT CALLBACK ViewportWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        SetFocus(hwnd);
        break;
    default:
        break;
    }
    return CallWindowProc(g_oldViewportProc, hwnd, msg, wp, lp);
}

void copyStr(char* dst, size_t dstSize, const std::string& src)
{
    std::strncpy(dst, src.c_str(), dstSize - 1);
    dst[dstSize - 1] = '\0';
}

} // namespace

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Presenter::Presenter() = default;

Presenter::~Presenter()
{
    stop();
}

bool Presenter::start(const std::string& resRoot, std::string* outError)
{
    if (running_.load()) return true;

    te::paths::setResRoot(resRoot);
    resRoot_     = te::paths::resRoot();
    modelPath_   = resolveModelInDir(resRoot_ + "models/", "cyber_room.obj");
    texturePath_ = resolveTextureInDir(resRoot_ + "textures/", "cyber_room.png");

    {
        std::lock_guard lk(initMutex_);
        initDone_ = false;
        initOk_   = false;
        initError_.clear();
    }

    renderThread_ = std::thread(&Presenter::threadMain, this);

    // Synchronously wait for the render thread to finish Vulkan init (up to 60s).
    std::unique_lock lk(initMutex_);
    const bool done = initCv_.wait_for(lk, std::chrono::seconds(60),
                                       [this] { return initDone_; });
    if (!done || !initOk_) {
        if (outError) *outError = done ? initError_ : "engine init timed out";
        running_ = false;
        if (window_) glfwSetWindowShouldClose(window_, GLFW_TRUE);
        if (renderThread_.joinable()) renderThread_.join();
        return false;
    }
    return true;
}

void Presenter::stop()
{
    running_ = false;
    if (window_) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
        // Wake glfwWaitEvents so the render thread observes the close request
        // immediately (both GLFW calls are documented as safe from any thread).
        glfwPostEmptyEvent();
    }
    if (renderThread_.joinable()) renderThread_.join();

    // Window teardown happens here, after the join, so stop() never touches a
    // dangling GLFWwindow while the render thread is destroying it.
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

void Presenter::log(int level, const std::string& msg) const
{
    if (logSink_) logSink_(level, msg);
}

// ─── Render thread ────────────────────────────────────────────────────────────

void Presenter::threadMain()
{
    try {
        threadInit();
    } catch (const std::exception& e) {
        {
            std::lock_guard lk(initMutex_);
            initDone_  = true;
            initOk_    = false;
            initError_ = e.what();
        }
        initCv_.notify_all();
        if (window_) { glfwDestroyWindow(window_); window_ = nullptr; }
        glfwTerminate();
        return;
    }

    {
        std::lock_guard lk(initMutex_);
        initDone_ = true;
        initOk_   = true;
    }
    initCv_.notify_all();
    log(0, "tinyEngine initialized (render thread).");

    running_ = true;
    float prevTick = -1.0f;

    while (running_.load() && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        const float now = static_cast<float>(glfwGetTime());
        const float dt  = (prevTick < 0.f) ? 0.f : glm::max(0.f, now - prevTick);
        prevTick = now;

        processInput();
        Camera& cam = runtime_.camera();
        if (!cam.IsSmoothFocusActive())
            cam.UpdataCameraPosition(dt > 0.f ? dt : 1.f / 240.f);
        cam.UpdateSmoothFocus(dt > 0.f ? dt : 1.f / 240.f);

        drainCommands();

        int fw = 0, fh = 0;
        glfwGetFramebufferSize(window_, &fw, &fh);
        if (fw > 0 && fh > 0) {
            try {
                renderer_.drawFrame(runtime_);
            } catch (const std::exception& e) {
                log(2, std::string("render loop fatal: ") + e.what());
                break;
            }
        } else {
            glfwWaitEventsTimeout(0.05);
        }

        publishSnapshot();
    }

    runtime_.destroy(renderer_);
    renderer_.destroy();
    // The GLFW window is destroyed by stop() after this thread has been joined.
    log(0, "tinyEngine shutdown complete.");
}

void Presenter::threadInit()
{
    // Env var setup must precede any Vulkan call so the loader picks up
    // RenderDoc's implicit capture layer at instance creation.
    tinyengine::debug::preInstanceRenderDocSetup();

    if (!glfwInit())
        throw std::runtime_error("glfwInit failed");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // shown after attaching to the host viewport
    window_ = glfwCreateWindow(WIDTH, HEIGHT, "tinyEngine Viewport", nullptr, nullptr);
    if (!window_)
        throw std::runtime_error("glfwCreateWindow failed");

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetCursorPosCallback(window_, mouseCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    const std::string vertSpv    = resRoot_ + "shaders/vert.spv";
    const std::string fragSpv    = resRoot_ + "shaders/frag.spv";
    const std::string boxVertSpv = resRoot_ + "shaders/box_vert.spv";
    const std::string boxFragSpv = resRoot_ + "shaders/box.spv";

    renderer_.initCore(window_, vertSpv, fragSpv, boxVertSpv, boxFragSpv);

    // Now that vkCreateInstance has run, the Vulkan loader may have loaded
    // renderdoc.dll as an implicit layer; bind to it if so.
    tinyengine::debug::initRenderDoc();
    // Emit the (now-complete) RenderDoc status, which by this point includes
    // the Vulkan layer enumeration report and any layer-enable retry outcome.
    log(0, tinyengine::debug::getRenderDocStatus());

    {
        const VkExtent2D ext = renderer_.swapChain().getExtent();
        runtime_.camera().SetAspectRatio(static_cast<float>(ext.width), static_cast<float>(ext.height));
    }

    runtime_.initScene(renderer_, modelPath_, texturePath_);
    renderer_.initFrameResources();

    registerDebugCommands();
}

// ─── Command queue ────────────────────────────────────────────────────────────

void Presenter::enqueue(Command cmd)
{
    std::lock_guard lk(cmdMutex_);
    cmdQueue_.push_back(std::move(cmd));
}

void Presenter::drainCommands()
{
    std::deque<Command> pending;
    {
        std::lock_guard lk(cmdMutex_);
        pending.swap(cmdQueue_);
    }
    for (const Command& cmd : pending)
        applyCommand(cmd);
}

void Presenter::applyCommand(const Command& cmd)
{
    std::visit([this](const auto& c) {
        using T = std::decay_t<decltype(c)>;

        if constexpr (std::is_same_v<T, CmdAttach>) {
            HWND hwnd = glfwGetWin32Window(window_);
            SetWindowLongPtr(hwnd, GWL_STYLE,
                             WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, 0);
            SetParent(hwnd, static_cast<HWND>(c.hwnd));
            if (!g_oldViewportProc)
                g_oldViewportProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
                    hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ViewportWndProc)));
            MoveWindow(hwnd, 0, 0, c.w, c.h, TRUE);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            ShowWindow(hwnd, SW_SHOW);
            renderer_.setFramebufferResized();
            log(0, "viewport attached.");
        } else if constexpr (std::is_same_v<T, CmdResize>) {
            if (window_ && c.w > 0 && c.h > 0) {
                HWND hwnd = glfwGetWin32Window(window_);
                MoveWindow(hwnd, 0, 0, c.w, c.h, TRUE);
            }
        } else if constexpr (std::is_same_v<T, CmdLoadModel>) {
            if (runtime_.loadModelFromRes(renderer_, c.path))
                log(0, "model loaded: " + c.path);
            else
                log(2, "model load failed: " + c.path);
        } else if constexpr (std::is_same_v<T, CmdLoadAst>) {
            if (runtime_.loadAndApplyMaterialAsset(renderer_, c.path))
                log(0, "material asset applied: " + c.path);
            else
                log(2, "material asset failed: " + c.path);
        } else if constexpr (std::is_same_v<T, CmdAddBox>) {
            runtime_.addBox(renderer_, c.id, glm::vec3(c.x, c.y, c.z));
        } else if constexpr (std::is_same_v<T, CmdRemoveBox>) {
            runtime_.removeBox(renderer_, c.id);
        } else if constexpr (std::is_same_v<T, CmdSetMatParams>) {
            MaterialParams p{};
            p.baseColor         = glm::vec4(c.baseColor[0], c.baseColor[1], c.baseColor[2], c.baseColor[3]);
            p.roughness         = c.roughness;
            p.metallic          = c.metallic;
            p.emissiveIntensity = c.emissiveIntensity;
            p.emissiveColor     = glm::vec4(c.emissiveColor[0], c.emissiveColor[1], c.emissiveColor[2], c.emissiveColor[3]);
            runtime_.setMaterialParams(c.id, p);
        } else if constexpr (std::is_same_v<T, CmdClearColor>) {
            runtime_.clearColor[0] = c.r;
            runtime_.clearColor[1] = c.g;
            runtime_.clearColor[2] = c.b;
            runtime_.clearColor[3] = c.a;
        } else if constexpr (std::is_same_v<T, CmdSelect>) {
            runtime_.select(c.what, c.boxId);
        } else if constexpr (std::is_same_v<T, CmdAssignMat>) {
            if (c.what == TE_SELECT_MODEL)      runtime_.setModelMaterial(c.materialId);
            else if (c.what == TE_SELECT_BOX)   runtime_.setBoxMaterial(c.boxId, c.materialId);
        } else if constexpr (std::is_same_v<T, CmdModelPos>) {
            runtime_.mainModelTransform.position = glm::vec3(c.x, c.y, c.z);
        } else if constexpr (std::is_same_v<T, CmdBoxPos>) {
            runtime_.setBoxPosition(renderer_, c.id, glm::vec3(c.x, c.y, c.z));
        } else if constexpr (std::is_same_v<T, CmdDebugInvoke>) {
            const std::string result =
                tinyengine::debugcmd::DebugCommandRegistry::instance().invoke(c.name, c.args);
            if (!result.empty())
                log(result.rfind("error", 0) == 0 ? 2 : 0, c.name + ": " + result);
        }
    }, cmd);
}

// ─── Command enqueue (thread-safe, called from the host thread) ──────────────

void Presenter::viewportAttach(void* hwndParent, int w, int h)
{
    enqueue(CmdAttach{ hwndParent, w, h });
}

void Presenter::viewportResize(int w, int h)
{
    enqueue(CmdResize{ w, h });
}

void Presenter::cmdLoadModel(const std::string& relPath)
{
    enqueue(CmdLoadModel{ relPath });
}

void Presenter::cmdLoadMaterialAst(const std::string& relPath)
{
    enqueue(CmdLoadAst{ relPath });
}

uint64_t Presenter::cmdAddBox(float x, float y, float z)
{
    const uint64_t id = nextBoxId_++;
    enqueue(CmdAddBox{ id, x, y, z });
    return id;
}

void Presenter::cmdRemoveBox(uint64_t id)
{
    enqueue(CmdRemoveBox{ id });
}

void Presenter::cmdSetMaterialParams(uint32_t id, const float* baseColor4,
                                     float roughness, float metallic,
                                     float emissiveIntensity, const float* emissiveColor4)
{
    CmdSetMatParams c{};
    c.id = id;
    if (baseColor4) std::memcpy(c.baseColor, baseColor4, sizeof(c.baseColor));
    else { c.baseColor[0] = c.baseColor[1] = c.baseColor[2] = c.baseColor[3] = 1.f; }
    c.roughness         = roughness;
    c.metallic          = metallic;
    c.emissiveIntensity = emissiveIntensity;
    if (emissiveColor4) std::memcpy(c.emissiveColor, emissiveColor4, sizeof(c.emissiveColor));
    else { c.emissiveColor[0] = c.emissiveColor[1] = c.emissiveColor[2] = 0.f; c.emissiveColor[3] = 1.f; }
    enqueue(c);
}

void Presenter::cmdSetClearColor(float r, float g, float b, float a)
{
    enqueue(CmdClearColor{ r, g, b, a });
}

void Presenter::cmdSelect(int what, uint64_t boxId)
{
    enqueue(CmdSelect{ what, boxId });
}

void Presenter::cmdAssignMaterial(int what, uint64_t boxId, uint32_t materialId)
{
    enqueue(CmdAssignMat{ what, boxId, materialId });
}

void Presenter::cmdSetModelPosition(float x, float y, float z)
{
    enqueue(CmdModelPos{ x, y, z });
}

void Presenter::cmdSetBoxPosition(uint64_t id, float x, float y, float z)
{
    enqueue(CmdBoxPos{ id, x, y, z });
}

void Presenter::cmdDebugInvoke(const std::string& name, std::vector<DebugArg> args)
{
    enqueue(CmdDebugInvoke{ name, std::move(args) });
}

// Populate the debug command registry. Commands run on the render thread (via
// the command queue) and touch runtime_/renderer_ directly, so it is safe to
// capture 'this' here. Registration itself happens once at init time.
void Presenter::registerDebugCommands()
{
    using namespace tinyengine::debugcmd;

    auto floatParam = [](const char* nm, double def, double lo, double hi) {
        DebugParamMeta p; p.type = DebugParamType::Float; p.name = nm;
        p.minVal = lo; p.maxVal = hi; p.defVal.type = DebugParamType::Float; p.defVal.f = def;
        return p;
    };
    auto colorParam = [](const char* nm, float r, float g, float b, float a) {
        DebugParamMeta p; p.type = DebugParamType::Color; p.name = nm;
        p.defVal.type = DebugParamType::Color;
        p.defVal.v[0] = r; p.defVal.v[1] = g; p.defVal.v[2] = b; p.defVal.v[3] = a;
        return p;
    };
    auto vec3Param = [](const char* nm, float x, float y, float z) {
        DebugParamMeta p; p.type = DebugParamType::Vec3; p.name = nm;
        p.defVal.type = DebugParamType::Vec3;
        p.defVal.v[0] = x; p.defVal.v[1] = y; p.defVal.v[2] = z;
        return p;
    };
    auto stringParam = [](const char* nm, const char* def) {
        DebugParamMeta p; p.type = DebugParamType::String; p.name = nm;
        p.defVal.type = DebugParamType::String; p.defVal.s = def ? def : "";
        return p;
    };
    auto u64Param = [](const char* nm) {
        DebugParamMeta p; p.type = DebugParamType::UInt64; p.name = nm; return p;
    };

    // render.set_clear_color(rgba)
    registerCommand<DebugColor>(
        "render.set_clear_color", "Set the viewport clear color (rgba).",
        { colorParam("color", 0.02f, 0.02f, 0.03f, 1.0f) },
        std::function<void(DebugColor)>([this](DebugColor c) {
            runtime_.clearColor[0] = c.r; runtime_.clearColor[1] = c.g;
            runtime_.clearColor[2] = c.b; runtime_.clearColor[3] = c.a;
        }));

    // scene.add_box(pos)
    registerCommand<DebugVec3>(
        "scene.add_box", "Spawn a box at the given world position.",
        { vec3Param("position", 0.0f, 0.0f, 0.0f) },
        std::function<void(DebugVec3)>([this](DebugVec3 p) {
            const uint64_t id = nextBoxId_++;
            runtime_.addBox(renderer_, id, glm::vec3(p.x, p.y, p.z));
            log(0, "added box id=" + std::to_string(id));
        }));

    // scene.remove_box(id)
    registerCommand<uint64_t>(
        "scene.remove_box", "Remove the box with the given entity id.",
        { u64Param("boxId") },
        std::function<void(uint64_t)>([this](uint64_t id) {
            runtime_.removeBox(renderer_, id);
        }));

    // scene.set_model_position(pos)
    registerCommand<DebugVec3>(
        "scene.set_model_position", "Move the main model to a world position.",
        { vec3Param("position", 0.0f, 0.0f, 0.0f) },
        std::function<void(DebugVec3)>([this](DebugVec3 p) {
            runtime_.mainModelTransform.position = glm::vec3(p.x, p.y, p.z);
        }));

    // scene.set_box_position(id, pos)
    registerCommand<uint64_t, DebugVec3>(
        "scene.set_box_position", "Move a box (by id) to a world position.",
        { u64Param("boxId"), vec3Param("position", 0.0f, 0.0f, 0.0f) },
        std::function<void(uint64_t, DebugVec3)>([this](uint64_t id, DebugVec3 p) {
            runtime_.setBoxPosition(renderer_, id, glm::vec3(p.x, p.y, p.z));
        }));

    // asset.load_model(relPath)
    registerCommand<std::string>(
        "asset.load_model", "Load a model by res-relative path (e.g. models/foo.obj).",
        { stringParam("relPath", "models/viking_room.obj") },
        std::function<void(std::string)>([this](std::string path) {
            if (runtime_.loadModelFromRes(renderer_, path)) log(0, "model loaded: " + path);
            else                                            log(2, "model load failed: " + path);
        }));

    // asset.load_material(relPath)
    registerCommand<std::string>(
        "asset.load_material", "Apply a material .ast by res-relative path.",
        { stringParam("relPath", "materials/viking_room.ast") },
        std::function<void(std::string)>([this](std::string path) {
            if (runtime_.loadAndApplyMaterialAsset(renderer_, path)) log(0, "material applied: " + path);
            else                                                     log(2, "material failed: " + path);
        }));

    // camera.set_speed(speed)
    registerCommand<float>(
        "camera.set_speed", "Set the free-fly camera movement speed.",
        { floatParam("speed", 3.0f, 0.1f, 50.0f) },
        std::function<void(float)>([this](float s) {
            runtime_.camera().SetSpeed(s);
        }));

    log(0, "debug commands registered: " +
           std::to_string(DebugCommandRegistry::instance().count()));
}


// ─── Input ────────────────────────────────────────────────────────────────────

void Presenter::processInput()
{
    static bool fKeyWasDown = false;
    const bool fKeyDown = glfwGetKey(window_, GLFW_KEY_F) == GLFW_PRESS;
    if (fKeyDown && !fKeyWasDown)
        runtime_.tryBeginCameraFocusOnPick();
    fKeyWasDown = fKeyDown;

    Camera& cam = runtime_.camera();
    cam.speedZ = (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) ?  1.f
               : (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) ? -1.f : 0.f;
    cam.speedX = (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) ? -1.f
               : (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) ?  1.f : 0.f;
}

void Presenter::framebufferResizeCallback(GLFWwindow* w, int, int)
{
    auto* self = static_cast<Presenter*>(glfwGetWindowUserPointer(w));
    if (self) self->renderer_.setFramebufferResized();
}

void Presenter::mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/)
{
    auto* self = static_cast<Presenter*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        self->rightMouseDown_ = (action == GLFW_PRESS);
        if (action == GLFW_RELEASE) self->firstMouse_ = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        double cx = 0.0, cy = 0.0;
        glfwGetCursorPos(w, &cx, &cy);
        self->runtime_.tryPickMainModel(self->renderer_, w,
                                        static_cast<float>(cx), static_cast<float>(cy));
    }
}

void Presenter::mouseCallback(GLFWwindow* w, double xpos, double ypos)
{
    auto* self = static_cast<Presenter*>(glfwGetWindowUserPointer(w));
    if (!self || !self->rightMouseDown_) return;

    if (self->firstMouse_) {
        self->lastX_ = static_cast<float>(xpos);
        self->lastY_ = static_cast<float>(ypos);
        self->firstMouse_ = false;
    }
    const float dx = static_cast<float>(xpos) - self->lastX_;
    const float dy = static_cast<float>(ypos) - self->lastY_;
    self->lastX_ = static_cast<float>(xpos);
    self->lastY_ = static_cast<float>(ypos);
    self->runtime_.camera().ProcessMouseMovement(dx, dy);
}

void Presenter::scrollCallback(GLFWwindow* w, double /*xoffset*/, double yoffset)
{
    auto* self = static_cast<Presenter*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    constexpr float kStep = 0.05f, kMin = 0.01f, kMax = 5.0f;
    Camera& cam = self->runtime_.camera();
    cam.SPEED = glm::clamp(cam.SPEED + static_cast<float>(yoffset) * kStep, kMin, kMax);
}

// ─── Snapshot ─────────────────────────────────────────────────────────────────

void Presenter::publishSnapshot()
{
    TeSceneSnapshot s{};

    s.pickedBoxEntityId  = runtime_.pickedBoxEntityId;
    s.selectedMaterialId = runtime_.selectedMaterialId;
    s.mainModelSelected  = runtime_.mainModelSelected ? 1 : 0;

    const ObjectTransform& t = runtime_.mainModelTransform;
    s.modelPosition[0] = t.position.x; s.modelPosition[1] = t.position.y; s.modelPosition[2] = t.position.z;
    s.modelRotation[0] = t.rotation.x; s.modelRotation[1] = t.rotation.y;
    s.modelRotation[2] = t.rotation.z; s.modelRotation[3] = t.rotation.w;
    s.modelScale[0]    = t.scale.x;    s.modelScale[1]    = t.scale.y;    s.modelScale[2]    = t.scale.z;

    const MaterialManager& matMgr = runtime_.materials();
    int n = 0;
    for (MaterialId id : matMgr.getAllMaterialIds()) {
        if (n >= TE_MAX_MATERIALS) break;
        TeMaterialInfo& mi = s.materials[n++];
        mi.id        = id;
        mi.type      = (matMgr.getMaterialType(id) == MaterialType::Box) ? 1 : 0;
        mi.deletable = matMgr.isDeletable(id) ? 1 : 0;
        copyStr(mi.name, TE_NAME_LEN, matMgr.getMaterialName(id));
        const MaterialParams& p = matMgr.getParams(id);
        mi.baseColor[0] = p.baseColor.x; mi.baseColor[1] = p.baseColor.y;
        mi.baseColor[2] = p.baseColor.z; mi.baseColor[3] = p.baseColor.w;
        mi.roughness         = p.roughness;
        mi.metallic          = p.metallic;
        mi.emissiveIntensity = p.emissiveIntensity;
        mi.emissiveColor[0] = p.emissiveColor.x; mi.emissiveColor[1] = p.emissiveColor.y;
        mi.emissiveColor[2] = p.emissiveColor.z; mi.emissiveColor[3] = p.emissiveColor.w;
    }
    s.materialCount = n;

    const SceneManager& scene = runtime_.scene();
    int b = 0;
    for (RenderEntityId id : scene.getBoxRangeEntityIds()) {
        if (b >= TE_MAX_BOXES) break;
        TeBoxInfo& bi = s.boxes[b++];
        bi.id = id;
        const glm::vec3 pos = scene.getBoxPosition(id);
        bi.position[0] = pos.x; bi.position[1] = pos.y; bi.position[2] = pos.z;
    }
    s.boxCount = b;

    std::memcpy(s.clearColor, runtime_.clearColor, sizeof(s.clearColor));

    const Camera& cam = runtime_.camera();
    s.cameraPosition[0] = cam.Position.x;
    s.cameraPosition[1] = cam.Position.y;
    s.cameraPosition[2] = cam.Position.z;
    s.cameraSpeed       = cam.SPEED;

    copyStr(s.modelName, TE_PATH_LEN,
            fs::path(runtime_.currentModelPath()).filename().string());

    std::lock_guard lk(snapMutex_);
    snap_ = s;
}

void Presenter::getSnapshot(TeSceneSnapshot& out) const
{
    std::lock_guard lk(snapMutex_);
    out = snap_;
}
