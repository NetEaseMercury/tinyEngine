#version 450
// PBR fragment shader — Cook-Torrance + GGX/Schlick/Smith.
// Framebuffer is sRGB (VK_FORMAT_B8G8R8A8_SRGB), so GPU does gamma encoding;
// we output linear color. Sampled textures (albedo/emissive) are bound as
// VK_FORMAT_R8G8B8A8_SRGB — GPU decodes to linear on sample.

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 materialTint;       // rgba multiplier for albedo
    vec4 boxMaterialTint;    // unused here
    vec4 emissive;           // rgb = emissive color * intensity
    vec4 cameraPos;          // xyz = world camera position
    vec4 lightDir;           // xyz = world-space *to-light* direction (normalized)
    vec4 lightColor;         // rgb = radiance, a = ambient strength
    vec4 pbrFactors;         // x=metallic, y=roughness, z=ao, w=normalScale
    // Pre-computed matrix cache (Phase 2 additions)
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
} ubo;

layout(binding = 1) uniform sampler2D albedoTex;
layout(binding = 2) uniform sampler2D normalTex;
layout(binding = 3) uniform sampler2D mrTex;       // g=roughness, b=metallic (glTF spec)
layout(binding = 4) uniform sampler2D aoTex;       // r=AO
layout(binding = 5) uniform sampler2D emissiveTex; // rgb=emissive

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) in vec3 vWorldNormal;
layout(location = 4) in vec4 vWorldTangent; // xyz=tangent, w=bitangent sign; (0,0,0,0)=absent

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float EPS = 1e-5;

// ─── BRDF building blocks ────────────────────────────────────────────────────
float D_GGX(float NdotH, float a) {
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, EPS);
}

float G_SchlickGGX(float NdotV, float k) {
    return NdotV / max(NdotV * (1.0 - k) + k, EPS);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    // direct-light remap
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}

vec3 F_Schlick(float HdotV, vec3 F0) {
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5.0);
}

// ─── Normal recovery ─────────────────────────────────────────────────────────
vec3 getNormal() {
    vec3 N = vWorldNormal;
    if (length(N) < EPS) {
        // No vertex normal — derive from screen-space derivatives of position.
        N = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
    } else {
        N = normalize(N);
    }

    // Sample tangent-space normal map. If texture is unbound it'll be the
    // 1x1 default (0.5,0.5,1.0) which decodes to (0,0,1) ⇒ identity.
    vec3 nSample = texture(normalTex, vUV).xyz * 2.0 - 1.0;
    nSample.xy  *= ubo.pbrFactors.w; // normalScale

    // If tangent missing (w==0 or zero-length) skip TBN perturbation.
    if (vWorldTangent.w == 0.0 || length(vWorldTangent.xyz) < EPS) {
        return N;
    }

    vec3 T = normalize(vWorldTangent.xyz - N * dot(vWorldTangent.xyz, N));
    vec3 B = normalize(cross(N, T) * vWorldTangent.w);
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * nSample);
}

// ─── Main ────────────────────────────────────────────────────────────────────
void main() {
    // Albedo: texture (sRGB→linear by GPU) × tint × vertex color
    vec4 albedoSample = texture(albedoTex, vUV);
    vec3 albedo = albedoSample.rgb * ubo.materialTint.rgb * vColor;
    float alpha = albedoSample.a * ubo.materialTint.a;

    // Metallic / Roughness — glTF convention: g=rough, b=metal
    vec3 mr = texture(mrTex, vUV).rgb;
    float roughness = clamp(mr.g * ubo.pbrFactors.y, 0.04, 1.0);
    float metallic  = clamp(mr.b * ubo.pbrFactors.x, 0.0, 1.0);
    float ao        = texture(aoTex, vUV).r * ubo.pbrFactors.z;

    vec3 N = getNormal();
    vec3 V = normalize(ubo.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(ubo.lightDir.xyz);
    vec3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // F0: dielectric 0.04, metallic uses albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float a = roughness * roughness;
    float D = D_GGX(NdotH, a);
    float G = G_Smith(NdotV, NdotL, roughness);
    vec3  F = F_Schlick(HdotV, F0);

    vec3 spec = (D * G * F) / max(4.0 * NdotV * NdotL, EPS);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 radiance = ubo.lightColor.rgb;
    vec3 Lo = (kD * albedo / PI + spec) * radiance * NdotL;

    // Ambient (very rough proxy — flat IBL surrogate)
    vec3 ambient = ubo.lightColor.a * albedo * ao;

    // Emissive: texture (sRGB→linear) × ubo.emissive (color*intensity)
    vec3 emissive = texture(emissiveTex, vUV).rgb * ubo.emissive.rgb;

    vec3 color = ambient + Lo + emissive;

    // Reinhard tonemap. Framebuffer sRGB will gamma-encode automatically.
    color = color / (color + vec3(1.0));

    outColor = vec4(color, alpha);
}
