#ifndef OBJECT_H
#define OBJECT_H

#include "core/gameObject.h"

struct Prop : GameObject{
  std::string modelPath;
  std::string texturePath;
}

#endif
