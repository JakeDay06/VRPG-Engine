#ifndef PLANE_H
#define PLANE_H

#include "gameObject.h"

class Plane : public GameObject {
  public:
    bool isCrashed = false;
    float planePitch = 0.0f;
    float planeYaw = 0.0f;
    float planeRoll = 0.0f;
    float planeSpeed = 0.0f;
    float maxSpeed = 20.0f;

    glm::mat4 planeOrientation = glm::mat4(1.0f);

    glm::mat4 getModelMatrix() const override {

    static const glm::mat4 meshCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(270.0f), glm::vec3(0.0f, 1.0f, 0.0f));
      glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
      model = model * planeOrientation * meshCorrection;
      model = glm::scale(model, scale);
      return model;
    }

    void crash(){
      isCrashed = true;
      planeSpeed = 0.0f;
      position.y = 0.0f;
      planePitch = 0.0f;
      planeYaw = 0.0f;
      planeRoll = 0.0f;
    }
};

#endif
