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
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);
}
