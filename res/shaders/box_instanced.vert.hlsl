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
    [[vk::location(0)]] float3 pos          : POSITION;
    [[vk::location(1)]] float3 color        : COLOR0;
    [[vk::location(2)]] float2 uv           : TEXCOORD0;
    [[vk::location(3)]] float4x4 instanceModel : INSTANCE_MATRIX;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float3 color : COLOR0;
    [[vk::location(1)]] float2 uv    : TEXCOORD0;
};

VSOutput main(VSInput input) {
    float4 wp = mul(input.instanceModel, float4(input.pos, 1.0));
    VSOutput output;
    output.pos   = mul(ubo.viewProj, wp);
    output.color = input.color;
    output.uv    = input.uv;
    return output;
}
