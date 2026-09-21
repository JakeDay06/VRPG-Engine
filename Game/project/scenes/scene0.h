#ifndef SCENE0_H
#define SCENE0_H

#include <glm/glm.hpp>

class Scene0 : Scene {
  public:
    void Scene0() {
      lights.push_back(
        Light{
        .position = glm::vec3{0.0f, 0.0f, 0.0f},
        .color = glm::vec3{1.0f, 1.0f, 1.0f}
        };
      );

      Player player = Player();
      entities.push_back(player);

      Ground ground = Ground();
      props.push_back(ground);
  }

};

#endif
