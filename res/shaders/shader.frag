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

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D normalSampler;

void main() {
    vec4 tex    = texture(texSampler, fragTexCoord);
    vec3 color  = tex.rgb * ubo.materialTint.rgb;
    vec3 emit   = ubo.emissive.rgb;
    outColor    = vec4(color + emit, tex.a);
}
