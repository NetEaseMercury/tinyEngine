using System;
using System.IO;
using System.Runtime.InteropServices;

namespace tinyEditor;

/// <summary>
/// P/Invoke wrapper for the tinyEngine C ABI. Mirrors src/tinyengine_api.h.
/// All write operations take effect asynchronously (engine command queue);
/// queries go through snapshot copies.
/// </summary>
public static class EngineApi
{
    private const string DllName = "tinyEngine.dll";

    public const int MaxMaterials = 64;
    public const int MaxBoxes = 256;

    public const int SelectNone = 0;
    public const int SelectModel = 1;
    public const int SelectBox = 2;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TeMaterialInfo
    {
        public uint id;
        public int type;        // 0 = Mesh, 1 = Box
        public int deletable;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string name;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public float[] baseColor;
        public float roughness;
        public float metallic;
        public float emissiveIntensity;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public float[] emissiveColor;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct TeBoxInfo
    {
        public ulong id;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)] public float[] position;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TeSceneSnapshot
    {
        public ulong pickedBoxEntityId;
        public uint selectedMaterialId;
        public int mainModelSelected;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)] public float[] modelPosition;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public float[] modelRotation;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)] public float[] modelScale;
        public int materialCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MaxMaterials)] public TeMaterialInfo[] materials;
        public int boxCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MaxBoxes)] public TeBoxInfo[] boxes;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public float[] clearColor;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)] public float[] cameraPosition;
        public float cameraSpeed;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)] public string modelName;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void TeLogCallback(int level, IntPtr msg);

    // ── Debug command reflection (mirrors src/tinyengine_api.h) ──────────────────
    public const int DbgParamInt = 0;
    public const int DbgParamFloat = 1;
    public const int DbgParamBool = 2;
    public const int DbgParamString = 3;
    public const int DbgParamEnum = 4;
    public const int DbgParamVec3 = 5;
    public const int DbgParamUInt64 = 6;
    public const int DbgParamColor = 7;

    public const int DbgMaxParams = 8;
    private const int DbgNameLen = 128;
    private const int DbgHelpLen = 512;
    private const int DbgEnumLen = 256;
    private const int DbgStringLen = 256;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TeDebugParamInfo
    {
        public int type;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = DbgNameLen)] public string name;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = DbgEnumLen)] public string enumValues;
        public double minVal;
        public double maxVal;
        public double defNum;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public float[] defVec;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = DbgStringLen)] public string defStr;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TeDebugCommandInfo
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = DbgNameLen)] public string name;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = DbgHelpLen)] public string help;
        public int paramCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = DbgMaxParams)] public TeDebugParamInfo[] params_;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TeDebugArg
    {
        public int type;
        public long i;
        public double f;
        public int b;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public float[] v;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = DbgStringLen)] public string s;
    }

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int te_init([MarshalAs(UnmanagedType.LPUTF8Str)] string resRoot);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void te_shutdown();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_viewport_attach(IntPtr hwndParent, int w, int h);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_viewport_resize(int w, int h);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_load_model([MarshalAs(UnmanagedType.LPUTF8Str)] string relPath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_load_material_ast([MarshalAs(UnmanagedType.LPUTF8Str)] string relPath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong te_add_box(float x, float y, float z);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_remove_box(ulong id);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_material_set_params(uint id,
        [In] float[] baseColor4, float roughness, float metallic,
        float emissiveIntensity, [In] float[] emissiveColor4);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_set_clear_color(float r, float g, float b, float a);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_select(int what, ulong boxId);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_assign_material(int what, ulong boxId, uint materialId);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_set_model_position(float x, float y, float z);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void te_set_box_position(ulong id, float x, float y, float z);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int te_get_snapshot(ref TeSceneSnapshot snapshot);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void te_set_log_callback(TeLogCallback cb);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int te_debug_command_count();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int te_debug_command_info(int index, ref TeDebugCommandInfo outInfo);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int te_debug_invoke([MarshalAs(UnmanagedType.LPUTF8Str)] string name,
                                              [In] TeDebugArg[] args, int argCount);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern int te_debug_capture_frame();

    // ── High-level wrappers ──────────────────────────────────────────────────────

    internal static bool IsInitialized { get; private set; }

    /// <summary>Engine log event (the callback fires on the render thread; subscribers must marshal to the UI thread themselves).</summary>
    internal static event Action<int, string>? LogReceived;

    private static TeLogCallback? logCbKeepAlive_;

    /// <summary>The res/ directory (under the Editor output directory, synced by the csproj after build).</summary>
    internal static string ResRoot => Path.Combine(AppContext.BaseDirectory, "res");

    /// <summary>Idempotently start the engine (synchronously waits for Vulkan init). Returns true on success.</summary>
    internal static bool EnsureInitialized()
    {
        if (IsInitialized) return true;

        logCbKeepAlive_ = (level, msgPtr) =>
        {
            var msg = Marshal.PtrToStringUTF8(msgPtr) ?? string.Empty;
            try { LogReceived?.Invoke(level, msg); } catch { /* log callbacks must never affect the engine */ }
        };
        te_set_log_callback(logCbKeepAlive_);

        int rc;
        try {
            rc = te_init(ResRoot);
        } catch (Exception ex) {
            LogReceived?.Invoke(2, "te_init P/Invoke failed: " + ex.Message);
            return false;
        }
        IsInitialized = rc == 0;
        return IsInitialized;
    }

    /// <summary>Pull the latest scene snapshot; returns false when the engine is not ready.</summary>
    internal static bool TryGetSnapshot(out TeSceneSnapshot snapshot)
    {
        snapshot = new TeSceneSnapshot();
        if (!IsInitialized) return false;
        return te_get_snapshot(ref snapshot) == 0;
    }

    internal static void Shutdown()
    {
        if (!IsInitialized) return;
        te_shutdown();
        IsInitialized = false;
    }

    // ── Debug command wrappers ───────────────────────────────────────────────────

    /// <summary>Enumerate all registered debug commands (empty if engine not ready).</summary>
    internal static System.Collections.Generic.List<TeDebugCommandInfo> GetDebugCommands()
    {
        var list = new System.Collections.Generic.List<TeDebugCommandInfo>();
        if (!IsInitialized) return list;
        int n;
        try { n = te_debug_command_count(); } catch { return list; }
        for (int i = 0; i < n; ++i)
        {
            var info = new TeDebugCommandInfo();
            if (te_debug_command_info(i, ref info) == 0)
                list.Add(info);
        }
        return list;
    }

    /// <summary>Queue a debug command for execution on the render thread.</summary>
    internal static void DebugInvoke(string name, TeDebugArg[] args)
    {
        if (!IsInitialized) return;
        try { te_debug_invoke(name, args, args?.Length ?? 0); }
        catch (Exception ex) { LogReceived?.Invoke(2, "te_debug_invoke failed: " + ex.Message); }
    }

    /// <summary>Trigger a RenderDoc capture of the next frame. Returns true if accepted.</summary>
    internal static bool CaptureFrame()
    {
        if (!IsInitialized) return false;
        try { return te_debug_capture_frame() == 0; }
        catch (Exception ex) { LogReceived?.Invoke(2, "te_debug_capture_frame failed: " + ex.Message); return false; }
    }
}
