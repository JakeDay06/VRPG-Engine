#ifndef ENTITY_H
#define ENTITY_H

#include "core/gameObject.h"

struct Entity : public GameObject{
  std::string texturePath;
};

#endif
