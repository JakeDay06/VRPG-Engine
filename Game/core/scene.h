#ifndef SCENE_H
#define SCENE_H

#include "core/prop.h"
#include "core/entity.h"
#include "core/light.h"
#include "core/camera.h"

struct Scene{
  std::vector<Prop> props;
  std::vector<Entity> entities;
  std::vector<Light> lights;
  Camera camera;
}

#endif
