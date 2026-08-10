using System;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Threading;

namespace tinyEditor;

/// <summary>
/// Main window: UE-style layout (Hierarchy / Inspector / Content Browser /
/// Log / viewport).
/// Data flow: writes call the C ABI directly (applied asynchronously via the
/// engine command queue); a DispatcherTimer pulls snapshots at 30Hz to refresh
/// Hierarchy/Inspector.
/// </summary>
public partial class MainWindow : Window
{
    private readonly DispatcherTimer snapshotTimer_;
    private EditorSettings settings_ = EditorSettings.Load();

    public MainWindow()
    {
        InitializeComponent();

        EngineApi.LogReceived += OnEngineLog;

        // Hierarchy → engine
        hierarchy.SelectionRequested += (what, boxId) => EngineApi.te_select(what, boxId);
        hierarchy.AddBoxRequested += (x, y, z) => EngineApi.te_add_box(x, y, z);
        hierarchy.RemoveBoxRequested += id => EngineApi.te_remove_box(id);

        // Inspector → engine
        inspector.ModelPositionApply += (x, y, z) => EngineApi.te_set_model_position(x, y, z);
        inspector.BoxPositionApply += (id, x, y, z) => EngineApi.te_set_box_position(id, x, y, z);
        inspector.MaterialParamsChanged += (id, bc, r, m, ei, ec)
            => EngineApi.te_material_set_params(id, bc, r, m, ei, ec);
        inspector.AssignRequested += (what, boxId, matId)
            => EngineApi.te_assign_material(what, boxId, matId);

        // Content Browser → engine
        contentBrowser.MaterialOpenRequested += rel => EngineApi.te_load_material_ast(rel);

        snapshotTimer_ = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(33) };
        snapshotTimer_.Tick += (_, _) =>
        {
            if (!EngineApi.TryGetSnapshot(out var snap)) return;
            hierarchy.UpdateFromSnapshot(in snap);
            inspector.UpdateFromSnapshot(in snap);
        };

        Loaded += (_, _) =>
        {
            contentBrowser.Refresh();
            snapshotTimer_.Start();
        };
        Closing += (_, _) =>
        {
            snapshotTimer_.Stop();
            EngineApi.LogReceived -= OnEngineLog;
            // Shut the engine down while the window tree is still intact. Doing
            // this in Closed would race WPF tearing down the hosted viewport
            // window: the render thread could enter swapchain recreation against
            // a dead surface and block forever, deadlocking the join in
            // te_shutdown and leaving a zombie process behind.
            EngineApi.Shutdown();
        };
    }

    private void OnEngineLog(int level, string msg) => logPanel.Append(level, msg);

    private ConsoleWindow? console_;

    private void OnOpenConsole(object sender, RoutedEventArgs e)
    {
        if (console_ is { IsLoaded: true })
        {
            console_.Activate();
            return;
        }
        console_ = new ConsoleWindow { Owner = this };
        console_.Closed += (_, _) => console_ = null;
        console_.Show();
    }

    private void OnCaptureFrame(object sender, RoutedEventArgs e)
    {
        string uiPath = settings_.RenderDocUiPath?.Trim() ?? "";
        bool haveUserPath = !string.IsNullOrEmpty(uiPath);

        // If the user pinned a path but it doesn't exist, warn and fall back to
        // the engine-driven LaunchReplayUI — don't skip the capture itself.
        if (haveUserPath && !File.Exists(uiPath))
        {
            logPanel.Append(2, $"RenderDoc: configured UI path does not exist: {uiPath}");
            logPanel.Append(1, "RenderDoc: falling back to built-in launcher.");
            haveUserPath = false;
        }

        if (!haveUserPath)
        {
            // Old behaviour: engine triggers capture + LaunchReplayUI().
            bool ok = EngineApi.CaptureFrame();
            logPanel.Append(ok ? 0 : 1,
                ok ? "RenderDoc: capturing next frame (built-in launcher)."
                   : "RenderDoc not available (renderdoc.dll not loaded).");
            return;
        }

        // With a user-supplied UI: trigger capture without the engine launching
        // anything, then poll briefly for the .rdc to appear and spawn the UI.
        if (!EngineApi.CaptureFrameNoUI())
        {
            logPanel.Append(1, "RenderDoc not available (renderdoc.dll not loaded).");
            return;
        }
        // Give the render thread a few frames to write the file before opening it.
        var poll = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(150) };
        int attempts = 0;
        string beforePath = EngineApi.GetLastCapturePath();
        poll.Tick += (_, _) =>
        {
            attempts++;
            string cur = EngineApi.GetLastCapturePath();
            if (!string.IsNullOrEmpty(cur) && cur != beforePath)
            {
                poll.Stop();
                LaunchQrenderdoc(uiPath, cur);
                return;
            }
            if (attempts >= 20)   // ~3s
            {
                poll.Stop();
                logPanel.Append(1, "RenderDoc: timed out waiting for .rdc; capture may still land under captures/.");
            }
        };
        poll.Start();
    }

    private void OnEditRenderDocPath(object sender, RoutedEventArgs e)
    {
        var dlg = new RenderDocPathDialog(settings_.RenderDocUiPath) { Owner = this };
        if (dlg.ShowDialog() == true)
        {
            settings_.RenderDocUiPath = dlg.ResultPath;
            settings_.Save();
            logPanel.Append(0, string.IsNullOrEmpty(settings_.RenderDocUiPath)
                ? "RenderDoc UI path cleared."
                : "RenderDoc UI path set to: " + settings_.RenderDocUiPath);
        }
    }

    private void LaunchQrenderdoc(string exe, string rdcPath)
    {
        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = exe,
                Arguments = "\"" + rdcPath + "\"",
                UseShellExecute = true,   // let the shell handle any spaces/quoting
            };
            Process.Start(psi);
            logPanel.Append(0, $"RenderDoc UI launched: {Path.GetFileName(exe)}  <-  {rdcPath}");
        }
        catch (Exception ex)
        {
            logPanel.Append(2, $"Failed to launch RenderDoc UI ({exe}): {ex.Message}");
        }
    }
}
