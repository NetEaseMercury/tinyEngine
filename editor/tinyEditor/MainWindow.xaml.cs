using System;
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
        bool ok = EngineApi.CaptureFrame();
        logPanel.Append(ok ? 0 : 1,
            ok ? "RenderDoc: capturing next frame."
               : "RenderDoc not available (renderdoc.dll not loaded).");
    }
}
