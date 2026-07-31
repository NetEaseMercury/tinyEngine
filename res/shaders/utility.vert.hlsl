// utility.vert.hlsl — shared VS for the editor utility pipelines (outline
// inflated shell / stencil mark / gizmo).
// Optionally extrudes vertices along model-space normals (thickness), then
// transforms via model -> viewProj.
// The push-constant layout must match solid_color.frag.hlsl exactly (148 bytes).

struct UtilityPush {
    float4x4 model;      // offset   0
    float4x4 viewProj;   // offset  64
    float4   color;      // offset 128 (unused in VS, kept for layout parity)
    float    thickness;  // offset 144: normal extrusion width (0 = no extrusion)
};
[[vk::push_constant]] UtilityPush push;

struct VSIn {
    [[vk::location(0)]] float3 pos    : POSITION;
    [[vk::location(4)]] float3 normal : NORMAL; // (0,0,0) when missing; extrusion degenerates to 0
};

struct VSOut {
    float4 pos : SV_Position;
};

VSOut main(VSIn input) {
    VSOut o;
    float3 p = input.pos + input.normal * push.thickness;
    o.pos = mul(push.viewProj, mul(push.model, float4(p, 1.0)));
    return o;
}
