#include "tinyengine_api.h"
#include "Presenter.hpp"

#include <memory>
#include <mutex>

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
