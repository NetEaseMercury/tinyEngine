using System;
using System.Text;
using System.Windows;
using System.Windows.Controls;

namespace tinyEditor.Panels;

/// <summary>
/// Log panel: receives engine callback logs (possibly from the render thread;
/// marshaled to the UI thread internally). Backed by a read-only TextBox so the
/// text can be selected and copied (Ctrl+C / right-click / Copy All).
/// </summary>
public partial class LogPanel : UserControl
{
    private const int MaxLines = 500;
    private int lineCount_;

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
        string line = $"[{tag}] {msg}";

        if (logText.Text.Length > 0)
            logText.AppendText(Environment.NewLine);
        logText.AppendText(line);
        lineCount_++;

        // Trim from the top when exceeding the cap.
        if (lineCount_ > MaxLines)
        {
            int firstNewline = logText.Text.IndexOf('\n');
            if (firstNewline >= 0)
            {
                logText.Text = logText.Text.Substring(firstNewline + 1);
                lineCount_--;
            }
        }

        logText.CaretIndex = logText.Text.Length;
        logText.ScrollToEnd();
    }

    private void OnClear(object sender, RoutedEventArgs e)
    {
        logText.Clear();
        lineCount_ = 0;
    }

    private void OnCopyAll(object sender, RoutedEventArgs e)
    {
        if (logText.Text.Length == 0) return;
        try { Clipboard.SetText(logText.Text); }
        catch { /* clipboard may be transiently locked by another process */ }
    }
}
