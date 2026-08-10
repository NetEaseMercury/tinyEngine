using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using Microsoft.Win32;

namespace tinyEditor;

/// <summary>
/// Small settings dialog that lets the user point at a qrenderdoc.exe. Result
/// is stored on ResultPath when the user hits OK.
/// </summary>
public partial class RenderDocPathDialog : Window
{
    public string ResultPath { get; private set; } = "";

    public RenderDocPathDialog(string initialPath)
    {
        InitializeComponent();
        pathBox.Text = initialPath ?? "";
        UpdateStatus();
    }

    private void OnBrowse(object sender, RoutedEventArgs e)
    {
        var dlg = new OpenFileDialog
        {
            Title = "Locate qrenderdoc.exe",
            Filter = "qrenderdoc.exe|qrenderdoc.exe|Executables (*.exe)|*.exe|All files (*.*)|*.*",
            CheckFileExists = true,
        };
        if (!string.IsNullOrEmpty(pathBox.Text))
        {
            try { dlg.InitialDirectory = Path.GetDirectoryName(pathBox.Text); }
            catch { /* ignore invalid path */ }
        }
        if (dlg.ShowDialog(this) == true)
        {
            pathBox.Text = dlg.FileName;
        }
    }

    private void OnPathChanged(object sender, TextChangedEventArgs e) => UpdateStatus();

    private void UpdateStatus()
    {
        string p = (pathBox.Text ?? "").Trim();
        if (string.IsNullOrEmpty(p))
        {
            statusText.Text = "(no path set — capture will use RenderDoc's built-in launcher as fallback)";
            statusText.Foreground = Brushes.Gray;
        }
        else if (File.Exists(p))
        {
            statusText.Text = "OK: file exists.";
            statusText.Foreground = Brushes.SeaGreen;
        }
        else
        {
            statusText.Text = "Warning: file does not exist.";
            statusText.Foreground = Brushes.IndianRed;
        }
    }

    private void OnOk(object sender, RoutedEventArgs e)
    {
        ResultPath = (pathBox.Text ?? "").Trim();
        DialogResult = true;
    }
}
