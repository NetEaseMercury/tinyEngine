// Push constants: model matrix + pre-computed normal matrix (128 bytes).
// Requires maxPushConstantsSize >= 128 (guaranteed on all desktop GPUs).
struct PushModel {
    float4x4 model;        // offset   0
    float4x4 normalMatrix; // offset  64
};
[[vk::push_constant]] PushModel pushModel;

[[vk::binding(0)]]
cbuffer UBO : register(b0) {
    float4x4 view;
    float4x4 proj;
    float4   materialTint;
    float4   boxMaterialTint;
    float4   emissive;
    float4   cameraPos;
    float4   lightDir;
    float4   lightColor;
    float4   pbrFactors;
    float4x4 viewProj;
    float4x4 invView;
    float4x4 invProj;
};

struct VSInput {
    [[vk::location(0)]] float3 pos    : POSITION;
    [[vk::location(1)]] float3 color  : COLOR0;
    [[vk::location(2)]] float2 uv     : TEXCOORD0;
    [[vk::location(4)]] float3 normal : NORMAL;
    [[vk::location(5)]] float4 tangent : TANGENT;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float3 color       : COLOR0;
    [[vk::location(1)]] float2 uv          : TEXCOORD0;
    [[vk::location(2)]] float3 worldPos    : TEXCOORD1;
    [[vk::location(3)]] float3 worldNormal : TEXCOORD2;
    [[vk::location(4)]] float4 worldTangent : TEXCOORD3;
};

VSOutput main(VSInput input) {
    float4 wp = mul(pushModel.model, float4(input.pos, 1.0));
    VSOutput output;
    output.worldPos    = wp.xyz;
    output.pos         = mul(ubo.viewProj, wp);
    output.color       = input.color;
    output.uv          = input.uv;

    float3x3 nMat      = (float3x3)pushModel.normalMatrix;
    output.worldNormal = mul(nMat, input.normal);
    output.worldTangent = float4(mul(nMat, input.tangent.xyz), input.tangent.w);
    return output;
}
