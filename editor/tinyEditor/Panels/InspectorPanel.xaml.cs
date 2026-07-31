using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace tinyEditor.Panels;

/// <summary>
/// Inspector panel: transform editing + material parameter editing + material
/// assignment for the selection. The 30Hz snapshot refresh avoids overwriting
/// controls the user is editing (refill only on selection change).
/// </summary>
public partial class InspectorPanel : UserControl
{
    public event Action<float, float, float>? ModelPositionApply;
    public event Action<ulong, float, float, float>? BoxPositionApply;
    /// <summary>materialId, baseColor[4], roughness, metallic, emissiveIntensity, emissiveColor[4]</summary>
    public event Action<uint, float[], float, float, float, float[]>? MaterialParamsChanged;
    /// <summary>what(TE_SELECT_*), boxId, materialId</summary>
    public event Action<int, ulong, uint>? AssignRequested;

    private bool suppress_;
    private int selWhat_ = EngineApi.SelectNone;
    private ulong selBoxId_;
    private int filledWhat_ = -1;      // selection state the transform fields currently reflect
    private ulong filledBoxId_;
    private uint editingMaterialId_;   // material currently selected in the material combo

    public InspectorPanel()
    {
        InitializeComponent();
    }

    public void UpdateFromSnapshot(in EngineApi.TeSceneSnapshot snap)
    {
        suppress_ = true;
        try
        {
            selWhat_ = snap.mainModelSelected != 0 ? EngineApi.SelectModel
                     : snap.pickedBoxEntityId != 0 ? EngineApi.SelectBox
                     : EngineApi.SelectNone;
            selBoxId_ = snap.pickedBoxEntityId;

            selectedLabel.Text = selWhat_ switch {
                EngineApi.SelectModel => $"Selected: Main Model ({snap.modelName})",
                EngineApi.SelectBox => $"Selected: Box #{selBoxId_}",
                _ => "Selected: None",
            };
            transformSection.Visibility = selWhat_ == EngineApi.SelectNone
                ? Visibility.Collapsed : Visibility.Visible;

            // Refill position only when the selected target changes, so ongoing input is not interrupted
            if (selWhat_ != filledWhat_ || selBoxId_ != filledBoxId_)
            {
                filledWhat_ = selWhat_;
                filledBoxId_ = selBoxId_;
                if (selWhat_ == EngineApi.SelectModel)
                    SetPos(snap.modelPosition[0], snap.modelPosition[1], snap.modelPosition[2]);
                else if (selWhat_ == EngineApi.SelectBox)
                {
                    for (int i = 0; i < snap.boxCount && i < EngineApi.MaxBoxes; i++)
                        if (snap.boxes[i].id == selBoxId_)
                        {
                            var p = snap.boxes[i].position;
                            SetPos(p[0], p[1], p[2]);
                            break;
                        }
                }
            }

            UpdateMaterialList(in snap);
        }
        finally
        {
            suppress_ = false;
        }
    }

    private void UpdateMaterialList(in EngineApi.TeSceneSnapshot snap)
    {
        var ids = new List<uint>();
        for (int i = 0; i < snap.materialCount && i < EngineApi.MaxMaterials; i++)
            ids.Add(snap.materials[i].id);

        var current = materialCombo.Items.OfType<MaterialItem>().Select(m => m.Id).ToList();
        if (!ids.SequenceEqual(current))
        {
            uint keep = editingMaterialId_;
            materialCombo.Items.Clear();
            foreach (var id in ids)
            {
                ref readonly var m = ref FindMaterial(in snap, id);
                string kind = m.type == 1 ? "Box" : "Mesh";
                materialCombo.Items.Add(new MaterialItem(id, $"{m.name}  [{kind} #{id}]"));
            }
            editingMaterialId_ = 0; // force re-selection from keep/snapshot
            int idx = ids.IndexOf(keep);
            materialCombo.SelectedIndex = idx >= 0 ? idx : (ids.Count > 0 ? 0 : -1);
        }

        // On first fill or when the engine-side selected material changes,
        // reflect the snapshot's selected material into the combo box
        if (editingMaterialId_ == 0 && snap.selectedMaterialId != 0)
        {
            int idx = ids.IndexOf(snap.selectedMaterialId);
            if (idx >= 0) materialCombo.SelectedIndex = idx;
        }

        // Parameter section: fill parameters when the selected material changes
        uint selId = (materialCombo.SelectedItem as MaterialItem)?.Id ?? 0;
        if (selId != editingMaterialId_)
        {
            editingMaterialId_ = selId;
            if (selId != 0) FillParams(FindMaterial(in snap, selId));
        }
        paramsSection.Visibility = editingMaterialId_ != 0 ? Visibility.Visible : Visibility.Collapsed;
    }

    private void FillParams(in EngineApi.TeMaterialInfo m)
    {
        baseR.Value = m.baseColor[0];
        baseG.Value = m.baseColor[1];
        baseB.Value = m.baseColor[2];
        roughness.Value = m.roughness;
        metallic.Value = m.metallic;
        emissiveIntensity.Value = Math.Clamp(m.emissiveIntensity, emissiveIntensity.Minimum, emissiveIntensity.Maximum);
        emissiveR.Value = m.emissiveColor[0];
        emissiveG.Value = m.emissiveColor[1];
        emissiveB.Value = m.emissiveColor[2];
    }

    private static ref readonly EngineApi.TeMaterialInfo FindMaterial(in EngineApi.TeSceneSnapshot snap, uint id)
    {
        for (int i = 0; i < snap.materialCount && i < EngineApi.MaxMaterials; i++)
            if (snap.materials[i].id == id)
                return ref snap.materials[i];
        return ref snap.materials[0];
    }

    private void SetPos(float x, float y, float z)
    {
        posX.Text = Fmt(x);
        posY.Text = Fmt(y);
        posZ.Text = Fmt(z);
    }

    private void OnMaterialComboChanged(object sender, SelectionChangedEventArgs e)
    {
        if (suppress_) return;
        uint selId = (materialCombo.SelectedItem as MaterialItem)?.Id ?? 0;
        if (selId == editingMaterialId_) return;
        editingMaterialId_ = selId;
        if (selId != 0 && EngineApi.TryGetSnapshot(out var snap))
        {
            suppress_ = true;
            try { FillParams(FindMaterial(in snap, selId)); } finally { suppress_ = false; }
        }
        paramsSection.Visibility = editingMaterialId_ != 0 ? Visibility.Visible : Visibility.Collapsed;
    }

    private void OnParamsChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (suppress_ || editingMaterialId_ == 0) return;
        float[] baseColor = { (float)baseR.Value, (float)baseG.Value, (float)baseB.Value, 1f };
        float[] emissive = { (float)emissiveR.Value, (float)emissiveG.Value, (float)emissiveB.Value, 1f };
        MaterialParamsChanged?.Invoke(editingMaterialId_, baseColor,
            (float)roughness.Value, (float)metallic.Value,
            (float)emissiveIntensity.Value, emissive);
    }

    private void OnApplyPosition(object sender, RoutedEventArgs e)
    {
        float x = Parse(posX.Text), y = Parse(posY.Text), z = Parse(posZ.Text);
        if (selWhat_ == EngineApi.SelectModel) ModelPositionApply?.Invoke(x, y, z);
        else if (selWhat_ == EngineApi.SelectBox) BoxPositionApply?.Invoke(selBoxId_, x, y, z);
    }

    private void OnAssign(object sender, RoutedEventArgs e)
    {
        if (selWhat_ != EngineApi.SelectNone && editingMaterialId_ != 0)
            AssignRequested?.Invoke(selWhat_, selBoxId_, editingMaterialId_);
    }

    private static float Parse(string s)
        => float.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out float v) ? v : 0f;

    private static string Fmt(float v) => v.ToString("0.###", CultureInfo.InvariantCulture);

    private sealed record MaterialItem(uint Id, string Label)
    {
        public override string ToString() => Label;
    }
}
