using System.Windows;

namespace tinyEditor;

public partial class App : Application
{
    protected override void OnExit(ExitEventArgs e)
    {
        EngineApi.Shutdown();
        base.OnExit(e);
    }
}
