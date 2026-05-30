#version 450
#extension GL_ARB_separate_shader_objects : enable

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
    // Pre-computed matrix cache (Phase 2 additions)
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
} ubo;

// Per-vertex attributes (binding = 0, vertex rate)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

// Per-instance model matrix (binding = 1, instance rate).
// A mat4 occupies 4 consecutive locations (each column = one vec4 attribute).
layout(location = 3) in mat4 instanceModel; // consumes locations 3, 4, 5, 6

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    // Use pre-multiplied viewProj to save one matrix multiplication per vertex.
    gl_Position  = ubo.viewProj * instanceModel * vec4(inPosition, 1.0);
    fragColor    = inColor;
    fragTexCoord = inTexCoord;
}
