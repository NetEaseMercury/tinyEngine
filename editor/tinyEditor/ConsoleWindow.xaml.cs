using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace tinyEditor;

/// <summary>
/// Floating debug console. Reflects the engine-registered debug commands into a
/// searchable, dotted-namespace tree; builds parameter controls per type; and
/// invokes the selected command with collected argument values.
/// </summary>
public partial class ConsoleWindow : Window
{
    private List<EngineApi.TeDebugCommandInfo> commands_ = new();
    private EngineApi.TeDebugCommandInfo? selected_;
    // Per-parameter value readers built when a command is selected.
    private readonly List<Func<EngineApi.TeDebugArg>> paramReaders_ = new();

    public ConsoleWindow()
    {
        InitializeComponent();
        EngineApi.LogReceived += OnEngineLog;
        Loaded += (_, _) => ReloadCommands();
        Closed += (_, _) => EngineApi.LogReceived -= OnEngineLog;
    }

    private void ReloadCommands()
    {
        commands_ = EngineApi.GetDebugCommands();
        RebuildTree(searchBox.Text);
    }

    private void OnSearchChanged(object sender, TextChangedEventArgs e) => RebuildTree(searchBox.Text);

    // Group commands by their dotted prefix; filter by a case-insensitive
    // substring match on the full command name.
    private void RebuildTree(string filter)
    {
        commandTree.Items.Clear();
        var groups = new Dictionary<string, TreeViewItem>();

        foreach (var cmd in commands_)
        {
            if (!string.IsNullOrEmpty(filter) &&
                cmd.name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0)
                continue;

            int dot = cmd.name.IndexOf('.');
            string group = dot > 0 ? cmd.name.Substring(0, dot) : "(global)";
            string leaf = dot > 0 ? cmd.name.Substring(dot + 1) : cmd.name;

            if (!groups.TryGetValue(group, out var groupItem))
            {
                groupItem = new TreeViewItem { Header = group, IsExpanded = true };
                groups[group] = groupItem;
                commandTree.Items.Add(groupItem);
            }
            groupItem.Items.Add(new TreeViewItem { Header = leaf, Tag = cmd.name });
        }
    }

    private void OnCommandSelected(object sender, RoutedPropertyChangedEventArgs<object> e)
    {
        selected_ = null;
        paramReaders_.Clear();
        paramPanel.Children.Clear();
        helpText.Text = string.Empty;
        runButton.IsEnabled = false;

        if (e.NewValue is not TreeViewItem item || item.Tag is not string cmdName) return;

        var cmd = commands_.FirstOrDefault(c => c.name == cmdName);
        if (cmd.name != cmdName) return;
        selected_ = cmd;
        helpText.Text = cmd.help;

        for (int i = 0; i < cmd.paramCount; ++i)
            BuildParamControl(cmd.params_[i]);

        runButton.IsEnabled = true;
    }

    // Build a labeled input control for one parameter and register a reader that
    // produces the corresponding TeDebugArg at run time.
    private void BuildParamControl(EngineApi.TeDebugParamInfo p)
    {
        var block = new StackPanel { Margin = new Thickness(0, 0, 0, 8) };
        block.Children.Add(new TextBlock { Text = $"{p.name}  ({TypeName(p.type)})", FontWeight = FontWeights.SemiBold });

        switch (p.type)
        {
            case EngineApi.DbgParamBool:
            {
                var cb = new CheckBox { IsChecked = p.defNum != 0.0, Margin = new Thickness(0, 2, 0, 0) };
                block.Children.Add(cb);
                paramReaders_.Add(() => new EngineApi.TeDebugArg { type = p.type, b = (cb.IsChecked ?? false) ? 1 : 0, v = new float[4], s = "" });
                break;
            }
            case EngineApi.DbgParamString:
            {
                var tb = new TextBox { Text = p.defStr ?? "" };
                block.Children.Add(tb);
                paramReaders_.Add(() => new EngineApi.TeDebugArg { type = p.type, v = new float[4], s = tb.Text ?? "" });
                break;
            }
            case EngineApi.DbgParamEnum:
            {
                var combo = new ComboBox();
                var opts = (p.enumValues ?? "").Split('|', StringSplitOptions.RemoveEmptyEntries);
                foreach (var o in opts) combo.Items.Add(o);
                combo.SelectedIndex = opts.Length > 0 ? (int)Math.Clamp(p.defNum, 0, opts.Length - 1) : -1;
                block.Children.Add(combo);
                paramReaders_.Add(() => new EngineApi.TeDebugArg { type = p.type, i = combo.SelectedIndex, v = new float[4], s = "" });
                break;
            }
            case EngineApi.DbgParamInt:
            case EngineApi.DbgParamUInt64:
            {
                var tb = new TextBox { Text = ((long)p.defNum).ToString(CultureInfo.InvariantCulture) };
                block.Children.Add(tb);
                int type = p.type;
                paramReaders_.Add(() =>
                {
                    long.TryParse(tb.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out long val);
                    return new EngineApi.TeDebugArg { type = type, i = val, v = new float[4], s = "" };
                });
                break;
            }
            case EngineApi.DbgParamFloat:
            {
                var tb = new TextBox { Text = p.defNum.ToString("0.###", CultureInfo.InvariantCulture) };
                block.Children.Add(tb);
                if (p.maxVal > p.minVal)
                {
                    var slider = new Slider { Minimum = p.minVal, Maximum = p.maxVal, Value = Math.Clamp(p.defNum, p.minVal, p.maxVal) };
                    slider.ValueChanged += (_, ev) => tb.Text = ev.NewValue.ToString("0.###", CultureInfo.InvariantCulture);
                    tb.TextChanged += (_, _) => { if (double.TryParse(tb.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double v) && v >= p.minVal && v <= p.maxVal) slider.Value = v; };
                    block.Children.Add(slider);
                }
                paramReaders_.Add(() =>
                {
                    double.TryParse(tb.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double val);
                    return new EngineApi.TeDebugArg { type = EngineApi.DbgParamFloat, f = val, v = new float[4], s = "" };
                });
                break;
            }
            case EngineApi.DbgParamVec3:
            {
                var boxes = AddVectorBoxes(block, new[] { "x", "y", "z" }, p.defVec, 3);
                paramReaders_.Add(() => new EngineApi.TeDebugArg { type = p.type, v = ReadVec(boxes, 3), s = "" });
                break;
            }
            case EngineApi.DbgParamColor:
            {
                var boxes = AddVectorBoxes(block, new[] { "r", "g", "b", "a" }, p.defVec, 4);
                paramReaders_.Add(() => new EngineApi.TeDebugArg { type = p.type, v = ReadVec(boxes, 4), s = "" });
                break;
            }
            default:
                block.Children.Add(new TextBlock { Text = "(unsupported type)", Foreground = Brushes.IndianRed });
                paramReaders_.Add(() => new EngineApi.TeDebugArg { type = p.type, v = new float[4], s = "" });
                break;
        }
        paramPanel.Children.Add(block);
    }

    private static TextBox[] AddVectorBoxes(StackPanel block, string[] labels, float[]? defs, int count)
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal };
        var boxes = new TextBox[count];
        for (int i = 0; i < count; ++i)
        {
            row.Children.Add(new TextBlock { Text = labels[i], Margin = new Thickness(0, 4, 4, 0) });
            float d = (defs != null && i < defs.Length) ? defs[i] : 0f;
            boxes[i] = new TextBox { Width = 60, Margin = new Thickness(0, 0, 8, 0), Text = d.ToString("0.###", CultureInfo.InvariantCulture) };
            row.Children.Add(boxes[i]);
        }
        block.Children.Add(row);
        return boxes;
    }

    private static float[] ReadVec(TextBox[] boxes, int count)
    {
        var v = new float[4];
        for (int i = 0; i < count && i < boxes.Length; ++i)
            float.TryParse(boxes[i].Text, NumberStyles.Float, CultureInfo.InvariantCulture, out v[i]);
        return v;
    }

    private void OnRun(object sender, RoutedEventArgs e)
    {
        if (selected_ is not { } cmd) return;
        var args = paramReaders_.Select(r => r()).ToArray();
        AppendOutput(0, "> " + cmd.name);
        EngineApi.DebugInvoke(cmd.name, args);
    }

    private void OnEngineLog(int level, string msg) => AppendOutput(level, msg);

    private void AppendOutput(int level, string msg)
    {
        if (!Dispatcher.CheckAccess()) { Dispatcher.BeginInvoke(() => AppendOutput(level, msg)); return; }
        var item = new ListBoxItem
        {
            Content = msg,
            Foreground = level switch { 1 => Brushes.DarkGoldenrod, 2 => Brushes.IndianRed, _ => Brushes.Black },
        };
        outputList.Items.Add(item);
        while (outputList.Items.Count > 500) outputList.Items.RemoveAt(0);
        outputList.ScrollIntoView(item);
    }

    private static string TypeName(int t) => t switch
    {
        EngineApi.DbgParamInt => "int",
        EngineApi.DbgParamFloat => "float",
        EngineApi.DbgParamBool => "bool",
        EngineApi.DbgParamString => "string",
        EngineApi.DbgParamEnum => "enum",
        EngineApi.DbgParamVec3 => "vec3",
        EngineApi.DbgParamUInt64 => "uint64",
        EngineApi.DbgParamColor => "color",
        _ => "?",
    };
}
