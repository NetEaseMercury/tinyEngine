#include "tinyengine_api.h"
#include "Presenter.hpp"
#include "DebugCommand.hpp"
#include "TinyEngineDebug.hpp"

#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace {

std::unique_ptr<Presenter> g_engine;
TeLogCallback              g_logCb = nullptr;
std::mutex                 g_logMutex;

void forwardLog(int level, const std::string& msg)
{
    TeLogCallback cb;
    {
        std::lock_guard lk(g_logMutex);
        cb = g_logCb;
    }
    if (cb) cb(level, msg.c_str());
}

} // namespace

// ─── Lifecycle ────────────────────────────────────────────────────────────────

int32_t te_init(const char* resRootUtf8)
{
    if (g_engine) return 0; // idempotent: already initialized counts as success

    auto engine = std::make_unique<Presenter>();
    engine->setLogSink([](int level, const std::string& msg) { forwardLog(level, msg); });

    std::string err;
    if (!engine->start(resRootUtf8 ? resRootUtf8 : "", &err)) {
        forwardLog(2, "te_init failed: " + err);
        return -1;
    }

    g_engine = std::move(engine);
    return 0;
}

void te_shutdown(void)
{
    if (!g_engine) return;
    g_engine->stop();
    g_engine.reset();
}

// ─── Viewport ─────────────────────────────────────────────────────────────────

void te_viewport_attach(void* hwndParent, int32_t w, int32_t h)
{
    if (g_engine && hwndParent) g_engine->viewportAttach(hwndParent, w, h);
}

void te_viewport_resize(int32_t w, int32_t h)
{
    if (g_engine) g_engine->viewportResize(w, h);
}

// ─── Scene commands ───────────────────────────────────────────────────────────

void te_load_model(const char* relPathUtf8)
{
    if (g_engine && relPathUtf8) g_engine->cmdLoadModel(relPathUtf8);
}

void te_load_material_ast(const char* relPathUtf8)
{
    if (g_engine && relPathUtf8) g_engine->cmdLoadMaterialAst(relPathUtf8);
}

uint64_t te_add_box(float x, float y, float z)
{
    if (!g_engine) return 0;
    return g_engine->cmdAddBox(x, y, z);
}

void te_remove_box(uint64_t id)
{
    if (g_engine) g_engine->cmdRemoveBox(id);
}

void te_material_set_params(uint32_t id,
                            const float* baseColor4,
                            float roughness, float metallic,
                            float emissiveIntensity,
                            const float* emissiveColor4)
{
    if (g_engine)
        g_engine->cmdSetMaterialParams(id, baseColor4, roughness, metallic,
                                       emissiveIntensity, emissiveColor4);
}

void te_set_clear_color(float r, float g, float b, float a)
{
    if (g_engine) g_engine->cmdSetClearColor(r, g, b, a);
}

void te_select(int32_t what, uint64_t boxId)
{
    if (g_engine) g_engine->cmdSelect(what, boxId);
}

void te_assign_material(int32_t what, uint64_t boxId, uint32_t materialId)
{
    if (g_engine) g_engine->cmdAssignMaterial(what, boxId, materialId);
}

void te_set_model_position(float x, float y, float z)
{
    if (g_engine) g_engine->cmdSetModelPosition(x, y, z);
}

void te_set_box_position(uint64_t id, float x, float y, float z)
{
    if (g_engine) g_engine->cmdSetBoxPosition(id, x, y, z);
}

// ─── Queries ──────────────────────────────────────────────────────────────────

int32_t te_get_snapshot(TeSceneSnapshot* out)
{
    if (!out) return -1;
    if (!g_engine) {
        *out = TeSceneSnapshot{};
        return -1;
    }
    g_engine->getSnapshot(*out);
    return 0;
}

// ─── Events ───────────────────────────────────────────────────────────────────

void te_set_log_callback(TeLogCallback cb)
{
    std::lock_guard lk(g_logMutex);
    g_logCb = cb;
}

// ─── Debug commands ─────────────────────────────────────────────────────────────

namespace {

void copyStr(char* dst, size_t cap, const std::string& src)
{
    const size_t n = src.size() < cap - 1 ? src.size() : cap - 1;
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

} // namespace

int32_t te_debug_command_count(void)
{
    return static_cast<int32_t>(tinyengine::debugcmd::DebugCommandRegistry::instance().count());
}

int32_t te_debug_command_info(int32_t index, TeDebugCommandInfo* out)
{
    using namespace tinyengine::debugcmd;
    if (!out) return -1;
    const DebugCommandEntry* e = DebugCommandRegistry::instance().at(static_cast<size_t>(index));
    if (!e) return -1;

    *out = TeDebugCommandInfo{};
    copyStr(out->name, TE_DBG_NAME_LEN, e->meta.name);
    copyStr(out->help, TE_DBG_HELP_LEN, e->meta.help);

    const int32_t pc = static_cast<int32_t>(e->meta.params.size());
    out->paramCount = pc < TE_DBG_MAX_PARAMS ? pc : TE_DBG_MAX_PARAMS;
    for (int32_t i = 0; i < out->paramCount; ++i) {
        const DebugParamMeta& m = e->meta.params[i];
        TeDebugParamInfo& p = out->params[i];
        p.type   = static_cast<int32_t>(m.type);
        copyStr(p.name, TE_DBG_NAME_LEN, m.name);
        copyStr(p.enumValues, TE_DBG_ENUM_LEN, m.enumValues);
        p.minVal = m.minVal;
        p.maxVal = m.maxVal;
        p.defNum = (m.defVal.type == DebugParamType::Float) ? m.defVal.f
                 : (m.defVal.type == DebugParamType::Bool)  ? (m.defVal.b ? 1.0 : 0.0)
                 : static_cast<double>(m.defVal.i);
        p.defVec[0] = m.defVal.v[0]; p.defVec[1] = m.defVal.v[1];
        p.defVec[2] = m.defVal.v[2]; p.defVec[3] = m.defVal.v[3];
        copyStr(p.defStr, TE_DBG_STRING_LEN, m.defVal.s);
    }
    return 0;
}

int32_t te_debug_invoke(const char* name, const TeDebugArg* args, int32_t argCount)
{
    if (!g_engine || !name) return -1;
    std::vector<DebugArg> converted;
    converted.reserve(argCount > 0 ? static_cast<size_t>(argCount) : 0);
    for (int32_t i = 0; i < argCount; ++i) {
        const TeDebugArg& a = args[i];
        DebugArg d;
        d.type = static_cast<DebugParamType>(a.type);
        d.i = a.i;
        d.f = a.f;
        d.b = a.b != 0;
        d.v[0] = a.v[0]; d.v[1] = a.v[1]; d.v[2] = a.v[2]; d.v[3] = a.v[3];
        d.s = a.s;
        converted.push_back(std::move(d));
    }
    g_engine->cmdDebugInvoke(name, std::move(converted));
    return 0;
}

int32_t te_debug_capture_frame(void)
{
    // TriggerCapture only sets a flag inside RenderDoc; safe to call from the
    // host (UI) thread. The capture is taken on the next present.
    if (!tinyengine::debug::triggerCapture()) {
        forwardLog(1, "RenderDoc not available (renderdoc.dll not loaded).");
        return -1;
    }
    const char* tmpl = tinyengine::debug::getCaptureFilePathTemplate();
    forwardLog(0, std::string("RenderDoc: capturing next frame -> ") +
                  (tmpl ? tmpl : "(default path)") + "_frameN.rdc");
    // Bring up the replay UI so the capture can be inspected immediately. It
    // connects back via target control and picks up the capture automatically.
    if (tinyengine::debug::launchReplayUI())
        forwardLog(0, "RenderDoc: replay UI launched/connected.");
    else
        forwardLog(1, "RenderDoc: could not launch replay UI (qrenderdoc not found?).");
    return 0;
}
