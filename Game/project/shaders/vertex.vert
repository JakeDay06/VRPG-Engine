#version 460

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;
layout(location = 4) out vec4 fragPosLightSpace;

layout(binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 projection;
  mat4 lightSpaceMatrix;
} ubo;

void main() {

    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPos, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = inNormal;
    fragPos = vec3(ubo.model * vec4(inPos, 1.0));
    fragPosLightSpace = ubo.lightSpaceMatrix * ubo.model * vec4(inPos, 1.0);
}
