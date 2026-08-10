using System;
using System.Windows;

namespace tinyEditor;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        // Enable RenderDoc's Vulkan capture layer for this process. Must be set
        // BEFORE the Vulkan loader (vulkan-1.dll) is first touched — i.e. before
        // any engine code runs — so the implicit layer is picked up by the
        // loader at instance creation. See renderdoc.json "enable_environment".
        Environment.SetEnvironmentVariable("ENABLE_VULKAN_RENDERDOC_CAPTURE", "1");
        base.OnStartup(e);
    }

    protected override void OnExit(ExitEventArgs e)
    {
        EngineApi.Shutdown();
        base.OnExit(e);
    }
}
