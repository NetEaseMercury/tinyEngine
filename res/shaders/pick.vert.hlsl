struct PickPush {
    float4x4 model;
    uint     objectId;
};
[[vk::push_constant]] PickPush push;

struct UBOData {
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
[[vk::binding(0)]]
ConstantBuffer<UBOData> ubo : register(b0);

struct VSInput {
    [[vk::location(0)]] float3 pos   : POSITION;
    [[vk::location(1)]] float3 color : COLOR0;
    [[vk::location(2)]] float2 uv    : TEXCOORD0;
};

struct VSOutput {
    float4 pos : SV_POSITION;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(push.model, float4(input.pos, 1.0))));
    return output;
}
