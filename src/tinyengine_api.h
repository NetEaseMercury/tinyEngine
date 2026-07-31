#pragma once

/*
 * tinyEngine C ABI — the single contract between the Editor and the engine.
 *
 * Conventions:
 *  - extern "C" functions + POD structs + UTF-8 strings only; no C++ types
 *    cross the boundary.
 *  - Write operations (te_load_model / te_load_material_ast / te_add_box, etc.)
 *    are asynchronous: they enter a command queue and are applied at the start
 *    of a render-thread frame.
 *  - Queries go through te_get_snapshot, which copies the read-only snapshot
 *    published by the engine at the end of each frame into the caller's buffer.
 *  - Events (logs/errors) are pushed via the callback registered with
 *    te_set_log_callback (the callback fires on the engine render thread; the
 *    host must marshal it back to the UI thread).
 *  - String paths are relative to the res/ directory (e.g. "materials/viking_room.ast").
 */

#include <stdint.h>

#ifdef _WIN32
  #ifdef TINYENGINE_EXPORTS
    #define TE_API __declspec(dllexport)
  #else
    #define TE_API __declspec(dllimport)
  #endif
  #define TE_CALL __cdecl
#else
  #define TE_API
  #define TE_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TE_MAX_MATERIALS 64
#define TE_MAX_BOXES     256
#define TE_NAME_LEN      128
#define TE_PATH_LEN      512

/* 'what' values for te_select / te_assign_material */
#define TE_SELECT_NONE   0
#define TE_SELECT_MODEL  1
#define TE_SELECT_BOX    2

typedef struct TeMaterialInfo {
    uint32_t id;
    int32_t  type;          /* 0 = Mesh, 1 = Box */
    int32_t  deletable;
    char     name[TE_NAME_LEN];
    float    baseColor[4];
    float    roughness;
    float    metallic;
    float    emissiveIntensity;
    float    emissiveColor[4];
} TeMaterialInfo;

typedef struct TeBoxInfo {
    uint64_t id;
    float    position[3];
} TeBoxInfo;

typedef struct TeSceneSnapshot {
    uint64_t pickedBoxEntityId;
    uint32_t selectedMaterialId;
    int32_t  mainModelSelected;
    float    modelPosition[3];
    float    modelRotation[4];  /* quaternion x,y,z,w */
    float    modelScale[3];
    int32_t  materialCount;
    TeMaterialInfo materials[TE_MAX_MATERIALS];
    int32_t  boxCount;
    TeBoxInfo boxes[TE_MAX_BOXES];
    float    clearColor[4];
    float    cameraPosition[3];
    float    cameraSpeed;
    char     modelName[TE_PATH_LEN];
} TeSceneSnapshot;

typedef void (TE_CALL *TeLogCallback)(int32_t level, const char* msg);

/* ── Lifecycle ─────────────────────────────────────────────────────────────
 * te_init: start the engine render thread and complete Vulkan initialization
 * (synchronously waited; returns 0 on success).
 * resRootUtf8: absolute path of the res/ directory (UTF-8); passing NULL falls
 * back to the relative path "res/".
 * te_shutdown: stop the render thread and release all resources (idempotent). */
TE_API int32_t te_init(const char* resRootUtf8);
TE_API void    te_shutdown(void);

/* ── Viewport ──────────────────────────────────────────────────────────────
 * te_viewport_attach: reparent the engine's GLFW window under the host handle
 * and show it.
 * te_viewport_resize: call when the host viewport size changes (triggers
 * swapchain recreation). */
TE_API void te_viewport_attach(void* hwndParent, int32_t w, int32_t h);
TE_API void te_viewport_resize(int32_t w, int32_t h);

/* ── Scene (async commands) ──────────────────────────────────────────────── */
TE_API void     te_load_model(const char* relPathUtf8);        /* e.g. "models/viking_room.obj" */
TE_API void     te_load_material_ast(const char* relPathUtf8); /* e.g. "materials/viking_room.ast" */
TE_API uint64_t te_add_box(float x, float y, float z);         /* returns a pre-allocated entity id */
TE_API void     te_remove_box(uint64_t id);
TE_API void     te_material_set_params(uint32_t id,
                                       const float* baseColor4,
                                       float roughness, float metallic,
                                       float emissiveIntensity,
                                       const float* emissiveColor4);
TE_API void     te_set_clear_color(float r, float g, float b, float a);
TE_API void     te_select(int32_t what, uint64_t boxId);       /* what: TE_SELECT_* */
TE_API void     te_assign_material(int32_t what, uint64_t boxId, uint32_t materialId);
TE_API void     te_set_model_position(float x, float y, float z);
TE_API void     te_set_box_position(uint64_t id, float x, float y, float z);

/* ── Queries ───────────────────────────────────────────────────────────────
 * Copy the latest snapshot into out (~18KB; safe to poll at 30Hz).
 * Returns -1 when the engine is not initialized. */
TE_API int32_t te_get_snapshot(TeSceneSnapshot* out);

/* ── Events ────────────────────────────────────────────────────────────────
 * Register the log callback (level: 0=info, 1=warn, 2=error). May be called
 * before te_init. */
TE_API void te_set_log_callback(TeLogCallback cb);

#ifdef __cplusplus
} /* extern "C" */
#endif
