using System;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace tinyEditor.Panels;

/// <summary>
/// Content browser: lists .ast material assets under res/materials;
/// double-click loads one (including linked model switching).
/// </summary>
public partial class ContentBrowserPanel : UserControl
{
    /// <summary>Raised on double-click of a .ast; the argument is the res/-relative path (e.g. "materials/viking_room.ast").</summary>
    public event Action<string>? MaterialOpenRequested;

    public ContentBrowserPanel()
    {
        InitializeComponent();
    }

    public void Refresh()
    {
        astList.Items.Clear();
        try
        {
            string dir = Path.Combine(EngineApi.ResRoot, "materials");
            if (!Directory.Exists(dir))
            {
                astList.Items.Add($"(missing: {dir})");
                return;
            }
            foreach (var file in Directory.GetFiles(dir, "*.ast"))
                astList.Items.Add(Path.GetFileName(file));
        }
        catch (Exception ex)
        {
            astList.Items.Add($"(scan failed: {ex.Message})");
        }
    }

    private void OnDoubleClick(object sender, MouseButtonEventArgs e)
    {
        if (astList.SelectedItem is not string name || name.StartsWith('(')) return;
        MaterialOpenRequested?.Invoke("materials/" + name);
    }

    private void OnRefresh(object sender, RoutedEventArgs e) => Refresh();
}
