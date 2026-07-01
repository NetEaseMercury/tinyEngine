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

struct PSInput {
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float3 color : COLOR0;
    [[vk::location(1)]] float2 uv    : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float3 base = float3(0.88, 0.88, 0.90) * ubo.boxMaterialTint.rgb;
    float3 emit = ubo.emissive.rgb;
    return float4(base + emit, 1.0);
}
