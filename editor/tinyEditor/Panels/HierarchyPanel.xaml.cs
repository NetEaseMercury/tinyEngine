using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace tinyEditor.Panels;

/// <summary>
/// Scene hierarchy panel: main model + box list, synced with the engine
/// selection state; supports adding/removing boxes.
/// </summary>
public partial class HierarchyPanel : UserControl
{
    /// <summary>Raised when the user clicks an entry to select it: what(0/1/2), boxId.</summary>
    public event Action<int, ulong>? SelectionRequested;
    public event Action<float, float, float>? AddBoxRequested;
    public event Action<ulong>? RemoveBoxRequested;

    private bool suppressEvents_;
    private readonly List<ulong> boxIds_ = new(); // maps to list items 1..N
    private string modelLabel_ = string.Empty;

    public HierarchyPanel()
    {
        InitializeComponent();
    }

    public void UpdateFromSnapshot(in EngineApi.TeSceneSnapshot snap)
    {
        suppressEvents_ = true;
        try
        {
            var newModelLabel = $"Main Model ({snap.modelName})";
            var newBoxIds = new List<ulong>();
            for (int i = 0; i < snap.boxCount && i < EngineApi.MaxBoxes; i++)
                newBoxIds.Add(snap.boxes[i].id);

            // Rebuild the list only on structural changes so the 30Hz refresh does
            // not interrupt the user's selection
            if (newModelLabel != modelLabel_ || !newBoxIds.SequenceEqual(boxIds_))
            {
                modelLabel_ = newModelLabel;
                boxIds_.Clear();
                boxIds_.AddRange(newBoxIds);

                entityList.Items.Clear();
                entityList.Items.Add(modelLabel_);
                foreach (var id in boxIds_)
                    entityList.Items.Add($"Box #{id}");
            }

            // Sync the engine-side selection state into the list
            int targetIndex = -1;
            if (snap.mainModelSelected != 0) targetIndex = 0;
            else if (snap.pickedBoxEntityId != 0)
            {
                int idx = boxIds_.IndexOf(snap.pickedBoxEntityId);
                if (idx >= 0) targetIndex = idx + 1;
            }
            if (entityList.SelectedIndex != targetIndex)
                entityList.SelectedIndex = targetIndex;
        }
        finally
        {
            suppressEvents_ = false;
        }
    }

    private void OnSelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (suppressEvents_) return;
        int idx = entityList.SelectedIndex;
        if (idx < 0) { SelectionRequested?.Invoke(EngineApi.SelectNone, 0); return; }
        if (idx == 0) { SelectionRequested?.Invoke(EngineApi.SelectModel, 0); return; }
        SelectionRequested?.Invoke(EngineApi.SelectBox, boxIds_[idx - 1]);
    }

    private void OnAddBox(object sender, RoutedEventArgs e)
    {
        float x = Parse(boxX.Text), y = Parse(boxY.Text), z = Parse(boxZ.Text);
        AddBoxRequested?.Invoke(x, y, z);
    }

    private void OnDeleteBox(object sender, RoutedEventArgs e)
    {
        int idx = entityList.SelectedIndex;
        if (idx > 0) RemoveBoxRequested?.Invoke(boxIds_[idx - 1]);
    }

    private static float Parse(string s)
        => float.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out float v) ? v : 0f;
}
