// PBR pixel shader -- Cook-Torrance + GGX/Schlick/Smith.
// Framebuffer is sRGB (VK_FORMAT_B8G8R8A8_SRGB), so GPU does gamma encoding;
// we output linear color. Sampled textures (albedo/emissive) are bound as
// VK_FORMAT_R8G8B8A8_SRGB -- GPU decodes to linear on sample.

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
    float4   pbrFactors;   // x=metallic, y=roughness, z=ao, w=normalScale
    float4x4 viewProj;
    float4x4 invView;
    float4x4 invProj;
};

[[vk::combinedImageSampler]][[vk::binding(1)]] Texture2D<float4> albedoTex    : register(t0);
[[vk::combinedImageSampler]][[vk::binding(1)]] SamplerState      albedoState  : register(s0);
[[vk::combinedImageSampler]][[vk::binding(2)]] Texture2D<float4> normalTex    : register(t1);
[[vk::combinedImageSampler]][[vk::binding(2)]] SamplerState      normalState  : register(s1);
[[vk::combinedImageSampler]][[vk::binding(3)]] Texture2D<float4> mrTex        : register(t2);
[[vk::combinedImageSampler]][[vk::binding(3)]] SamplerState      mrState      : register(s2);
[[vk::combinedImageSampler]][[vk::binding(4)]] Texture2D<float4> aoTex        : register(t3);
[[vk::combinedImageSampler]][[vk::binding(4)]] SamplerState      aoState      : register(s3);
[[vk::combinedImageSampler]][[vk::binding(5)]] Texture2D<float4> emissiveTex  : register(t4);
[[vk::combinedImageSampler]][[vk::binding(5)]] SamplerState      emissiveState : register(s4);

struct PSInput {
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float3 color       : COLOR0;
    [[vk::location(1)]] float2 uv          : TEXCOORD0;
    [[vk::location(2)]] float3 worldPos    : TEXCOORD1;
    [[vk::location(3)]] float3 worldNormal : TEXCOORD2;
    [[vk::location(4)]] float4 worldTangent : TEXCOORD3;
};

static const float PI  = 3.14159265359;
static const float EPS = 1e-5;

float D_GGX(float NdotH, float a) {
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, EPS);
}

float G_SchlickGGX(float NdotV, float k) {
    return NdotV / max(NdotV * (1.0 - k) + k, EPS);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}

float3 F_Schlick(float HdotV, float3 F0) {
    return F0 + (float3(1.0, 1.0, 1.0) - F0) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5.0);
}

float3 getNormal(float3 worldPos, float3 worldNormal, float4 worldTangent, float2 uv) {
    float3 N = worldNormal;
    if (length(N) < EPS) {
        N = normalize(cross(ddx(worldPos), ddy(worldPos)));
    } else {
        N = normalize(N);
    }

    float3 nSample = normalTex.Sample(normalState, uv).xyz * 2.0 - 1.0;
    nSample.xy *= ubo.pbrFactors.w;

    if (worldTangent.w == 0.0 || length(worldTangent.xyz) < EPS) {
        return N;
    }

    float3 T = normalize(worldTangent.xyz - N * dot(worldTangent.xyz, N));
    float3 B = normalize(cross(N, T) * worldTangent.w);
    float3x3 TBN = { T, B, N };
    return normalize(mul(nSample, TBN));
}

float4 main(PSInput input) : SV_TARGET {
    float4 albedoSample = albedoTex.Sample(albedoState, input.uv);
    float3 albedo = albedoSample.rgb * ubo.materialTint.rgb * input.color;
    float  alpha  = albedoSample.a   * ubo.materialTint.a;

    float3 mr       = mrTex.Sample(mrState, input.uv).rgb;
    float  roughness = clamp(mr.g * ubo.pbrFactors.y, 0.04, 1.0);
    float  metallic  = clamp(mr.b * ubo.pbrFactors.x, 0.0,  1.0);
    float  ao        = aoTex.Sample(aoState, input.uv).r * ubo.pbrFactors.z;

    float3 N = getNormal(input.worldPos, input.worldNormal, input.worldTangent, input.uv);
    float3 V = normalize(ubo.cameraPos.xyz - input.worldPos);
    float3 L = normalize(ubo.lightDir.xyz);
    float3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float a = roughness * roughness;
    float D = D_GGX(NdotH, a);
    float G = G_Smith(NdotV, NdotL, roughness);
    float3 F = F_Schlick(HdotV, F0);

    float3 spec = (D * G * F) / max(4.0 * NdotV * NdotL, EPS);

    float3 kS = F;
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - metallic);

    float3 radiance = ubo.lightColor.rgb;
    float3 Lo = (kD * albedo / PI + spec) * radiance * NdotL;

    float3 ambient = ubo.lightColor.a * albedo * ao;

    float3 emissive = emissiveTex.Sample(emissiveState, input.uv).rgb * ubo.emissive.rgb;

    float3 color = ambient + Lo + emissive;

    // Reinhard tonemap. Framebuffer sRGB will gamma-encode automatically.
    color = color / (color + float3(1.0, 1.0, 1.0));

    return float4(color, alpha);
}
