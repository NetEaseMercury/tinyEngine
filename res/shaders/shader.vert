#version 450
#extension GL_ARB_separate_shader_objects : enable

// 描述符集绑定的信息
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// 从vertex中读取的信息
layout(location = 0) in vec3 inPosition;  
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

// 输出到frag的信息
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
