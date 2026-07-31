using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace tinyEditor;

/// <summary>
/// Viewport host: creates a Win32 child window for the engine's GLFW window
/// to reparent into. The engine render thread presents the swapchain directly
/// to this child window, never blocking WPF (and vice versa).
/// </summary>
public class ViewportHost : HwndHost
{
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern IntPtr CreateWindowEx(int exStyle, string className, string windowName,
        int style, int x, int y, int width, int height,
        IntPtr parent, IntPtr menu, IntPtr instance, IntPtr param);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool DestroyWindow(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern bool MoveWindow(IntPtr hwnd, int x, int y, int w, int h, bool repaint);

    private const int WS_CHILD        = 0x40000000;
    private const int WS_VISIBLE      = 0x10000000;
    private const int WS_CLIPCHILDREN = 0x02000000;

    private IntPtr hostHwnd_;
    private bool attached_;
    private int lastW_ = -1, lastH_ = -1;

    protected override HandleRef BuildWindowCore(HandleRef hwndParent)
    {
        hostHwnd_ = CreateWindowEx(0, "Static", "tinyViewport",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 800, 600, hwndParent.Handle, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);

        // Engine init is synchronous (includes Vulkan init, a few seconds);
        // the viewport is attached after it succeeds.
        if (EngineApi.EnsureInitialized())
        {
            EngineApi.te_viewport_attach(hostHwnd_, 800, 600);
            attached_ = true;
        }
        return new HandleRef(this, hostHwnd_);
    }

    protected override void DestroyWindowCore(HandleRef hwnd)
    {
        attached_ = false;
        DestroyWindow(hwnd.Handle);
        hostHwnd_ = IntPtr.Zero;
    }

    protected override void OnRenderSizeChanged(SizeChangedInfo sizeInfo)
    {
        base.OnRenderSizeChanged(sizeInfo);
        if (!attached_ || hostHwnd_ == IntPtr.Zero) return;

        int w = Math.Max(1, (int)sizeInfo.NewSize.Width);
        int h = Math.Max(1, (int)sizeInfo.NewSize.Height);
        if (w == lastW_ && h == lastH_) return;
        lastW_ = w; lastH_ = h;

        MoveWindow(hostHwnd_, 0, 0, w, h, true);
        EngineApi.te_viewport_resize(w, h);
    }
}
