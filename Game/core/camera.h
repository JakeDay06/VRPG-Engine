#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "gameObject.h"

enum CameraMovement{
  MOVE_FORWARD,
  MOVE_BACKWARD,
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_UP,
  MOVE_DOWN,
  ROTATE_UP,
  ROTATE_DOWN,
  ROTATE_LEFT,
  ROTATE_RIGHT
};

enum CameraState{
  FREECAM,
  FIRSTPERSON,
  ORBIT
};

const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 5.0f;
const float ZOOM = 45.0f;


class Camera{
  public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float movementSpeed;
    float zoom;
    float camHeight;
    enum CameraState state;

    Camera(CameraState state = FREECAM, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f)){
      this->state = state;
      this->position = position;
      this->camHeight = position.y;
      this->worldUp = up;
      this->yaw = YAW;
      this->pitch = PITCH;
      this->front = glm::vec3(0.0f, 0.0f, -1.0f);
      this->movementSpeed = SPEED;
      this->zoom = ZOOM;
      updateCameraVectors();
    }

    void setCameraState(CameraState state){
      this->state = state;
      if(state == FIRSTPERSON){
        this->position.y = camHeight;
      }
    }

    glm::mat4 getViewMatrix(){
      return glm::lookAt(position, position + front, up);
    }

    void processMovement(CameraMovement direction, float deltaTime){
      float velocity = movementSpeed * deltaTime;

      if(state == FREECAM){
        if(direction == MOVE_FORWARD){
          position += front * velocity;
        }
        if(direction == MOVE_BACKWARD){
          position -= front * velocity;
        }
        if(direction == MOVE_LEFT){
          position -= right * velocity;
        }
        if(direction == MOVE_RIGHT){
          position += right * velocity;
        }
        if(direction == MOVE_UP){
          position += worldUp * velocity;
        }
        if(direction == MOVE_DOWN){
          position -= worldUp * velocity;
        }
      }

      if(state == FIRSTPERSON){
        glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z)); // normalize to maintain movement speed when looking at different angles
        if(direction == MOVE_FORWARD){
          position += flatFront * velocity;
        }
        if(direction == MOVE_BACKWARD){
          position -= flatFront * velocity;
        }
        if(direction == MOVE_LEFT){
          position -= right * velocity;
        }
        if(direction == MOVE_RIGHT){
          position += right * velocity;
        }
      }
    }

    void processRotation(CameraMovement direction, float deltaTime){
      float rotationSpeed = 75.0f * deltaTime;
      if(state != ORBIT){
        if(direction == ROTATE_UP){
          pitch += rotationSpeed;
        }
        if(direction == ROTATE_DOWN){
          pitch -= rotationSpeed;
        }
        if(direction == ROTATE_LEFT){
          yaw -= rotationSpeed;
        }
        if(direction == ROTATE_RIGHT){
          yaw += rotationSpeed;
        }

        if(pitch > 89.9f){
          pitch = 89.9f;
        }
        if(pitch < -89.9f){
          pitch = -89.9f;
        }

        updateCameraVectors();
      }
    }

    void lookAt(glm::vec3 target) {
      glm::vec3 direction = glm::normalize(target - position);
      pitch = glm::degrees(asin(direction.y));
      yaw = glm::degrees(atan2(direction.z, direction.x));
      updateCameraVectors();
    }

    void followObject(const auto& objPTR, glm::vec3 objForward, float distance, float posAngle, float deltaTime){
      auto& obj = *objPTR;

      glm::vec3 max = obj.vertices[0].pos, min = obj.vertices[0].pos;

      for (Vertex vert : obj.vertices){
        min = glm::min(min, vert.pos);
        max = glm::max(max, vert.pos);
      }

      glm::vec3 localCenter = (min + max) * 0.5f;

      glm::vec3 center = glm::vec3(obj.getModelMatrix() * glm::vec4(localCenter, 1.0f));

      glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(posAngle), glm::vec3(0.0f, 1.0f, 0.0f));
      glm::vec3 rotatedForward = glm::vec3(rot * glm::vec4(objForward, 0.0f));

      glm::vec3 followPosition = center - rotatedForward * distance;
      float smoothSpeed = 5.0f;

      float t = 1.0f - std::exp(-smoothSpeed * deltaTime);

      position = glm::mix(position, followPosition, t);
        this->lookAt(glm::vec3{center});
      }


  private:
    void updateCameraVectors(){
      glm::vec3 newFront;
      newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
      newFront.y = sin(glm::radians(pitch));
      newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
      front = glm::normalize(newFront);
      right = glm::normalize(glm::cross(front, worldUp));
      up = glm::normalize(glm::cross(right, front));
    }

};
#endif

