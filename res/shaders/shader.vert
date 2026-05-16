#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushModel {
    mat4 model;
} pushModel;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 materialTint;
    vec4 boxMaterialTint;
    vec4 emissive;
    vec4 cameraPos;
    vec4 lightDir;
    vec4 lightColor;
    vec4 pbrFactors;
} ubo;

// Vertex attributes (location=3 reserved for per-instance position used by box pipeline)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 4) in vec3 inNormal;
layout(location = 5) in vec4 inTangent; // xyz=tangent, w=bitangent sign

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec3 vWorldPos;
layout(location = 3) out vec3 vWorldNormal;
layout(location = 4) out vec4 vWorldTangent; // xyz transformed, w bitangent sign carried

void main() {
    vec4 wp     = pushModel.model * vec4(inPosition, 1.0);
    vWorldPos   = wp.xyz;
    gl_Position = ubo.proj * ubo.view * wp;

    vColor = inColor;
    vUV    = inTexCoord;

    // Use inverse-transpose for normals to handle non-uniform scale.
    mat3 nMat = transpose(inverse(mat3(pushModel.model)));
    vWorldNormal = nMat * inNormal;
    vWorldTangent = vec4(nMat * inTangent.xyz, inTangent.w);
}
