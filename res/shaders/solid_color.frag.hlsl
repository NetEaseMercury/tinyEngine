// solid_color.frag.hlsl — shared FS for the editor utility pipelines: outputs
// the solid color from push constants.
// The push-constant layout must match utility.vert.hlsl exactly (148 bytes).

struct UtilityPush {
    float4x4 model;      // offset   0 (unused in FS, kept for layout parity)
    float4x4 viewProj;   // offset  64
    float4   color;      // offset 128
    float    thickness;  // offset 144 (unused in FS)
};
[[vk::push_constant]] UtilityPush push;

float4 main() : SV_Target {
    return push.color;
}
