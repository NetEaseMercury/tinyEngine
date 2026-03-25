#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out uint outId;

layout(push_constant) uniform Push {
    mat4 model;
    uint objectId;
} push;

void main() {
    outId = push.objectId;
}
