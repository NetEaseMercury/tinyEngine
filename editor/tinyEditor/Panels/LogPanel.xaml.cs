using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace tinyEditor.Panels;

/// <summary>
/// Log panel: receives engine callback logs (possibly from the render thread;
/// marshaled to the UI thread internally).
/// </summary>
public partial class LogPanel : UserControl
{
    private const int MaxLines = 500;

    public LogPanel()
    {
        InitializeComponent();
    }

    public void Append(int level, string msg)
    {
        if (!Dispatcher.CheckAccess())
        {
            Dispatcher.BeginInvoke(() => Append(level, msg));
            return;
        }

        string tag = level switch { 1 => "WARN", 2 => "ERR ", _ => "INFO" };
        var item = new ListBoxItem { Content = $"[{tag}] {msg}" };
        item.Foreground = level switch {
            1 => Brushes.DarkGoldenrod,
            2 => Brushes.IndianRed,
            _ => Brushes.Black,
        };
        logList.Items.Add(item);

        while (logList.Items.Count > MaxLines)
            logList.Items.RemoveAt(0);

        logList.ScrollIntoView(item);
    }

    private void OnClear(object sender, RoutedEventArgs e) => logList.Items.Clear();
}
