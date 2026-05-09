#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 materialTint;
    vec4 boxMaterialTint;
    vec4 emissive;       // rgb = pre-multiplied emissive, w = intensity
} ubo;

void main() {
    vec3 base  = vec3(0.88, 0.88, 0.90) * ubo.boxMaterialTint.rgb;
    vec3 emit  = ubo.emissive.rgb;
    outColor   = vec4(base + emit, 1.0);
}
