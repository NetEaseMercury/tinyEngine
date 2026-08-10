using System;
using System.IO;
using System.Text.Json;

namespace tinyEditor;

/// <summary>
/// User-scoped editor preferences, persisted at
/// %APPDATA%\tinyEngine\editor.json. Currently holds the RenderDoc UI path so
/// captures can be opened in an existing qrenderdoc.exe without guessing.
/// </summary>
public sealed class EditorSettings
{
    // Fields the JSON round-trips. Public for System.Text.Json.
    public string RenderDocUiPath { get; set; } = "";

    private static readonly JsonSerializerOptions JsonOpts = new() { WriteIndented = true };

    public static string ConfigDir =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "tinyEngine");

    public static string ConfigPath => Path.Combine(ConfigDir, "editor.json");

    public static EditorSettings Load()
    {
        try
        {
            if (File.Exists(ConfigPath))
            {
                var json = File.ReadAllText(ConfigPath);
                var s = JsonSerializer.Deserialize<EditorSettings>(json);
                if (s != null) return s;
            }
        }
        catch { /* fall through to defaults on any read/parse error */ }

        var fresh = new EditorSettings();
        // First-run: try to auto-detect a system qrenderdoc.exe so the user only
        // needs to confirm rather than hunt through their disk.
        fresh.RenderDocUiPath = DetectQrenderdocPath() ?? "";
        return fresh;
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(ConfigDir);
            File.WriteAllText(ConfigPath, JsonSerializer.Serialize(this, JsonOpts));
        }
        catch (Exception ex)
        {
            // Non-fatal: settings are best-effort. Surface via Debug output;
            // MainWindow will still work with in-memory state until next launch.
            System.Diagnostics.Debug.WriteLine($"EditorSettings: failed to save {ConfigPath}: {ex.Message}");
        }
    }

    /// <summary>Try common install locations for qrenderdoc.exe. Returns null if none exist.</summary>
    public static string? DetectQrenderdocPath()
    {
        string[] candidates =
        {
            @"C:\Program Files\RenderDoc\qrenderdoc.exe",
            @"C:\Program Files (x86)\RenderDoc\qrenderdoc.exe",
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                         "Programs", "RenderDoc", "qrenderdoc.exe"),
        };
        foreach (var p in candidates)
            if (File.Exists(p)) return p;
        return null;
    }
}
