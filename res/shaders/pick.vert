#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Push {
    mat4 model;
    uint objectId;
} push;

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
    // Pre-computed matrix cache (Phase 2 additions — must match CPU UBO layout)
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);
}
