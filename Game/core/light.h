#ifndef LIGHT_H
#define LIGHT_H

struct Light {
  alignas(16) glm::vec3 position = glm::vec3{0.0f, 0.0f, 0.0f};
  alignas(16) glm::vec3 color = glm::vec3{0.0f, 0.0f, 0.0f};
};

#endif
