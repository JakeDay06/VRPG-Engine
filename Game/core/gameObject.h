#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#define VEC1 vk::Format::eR32Sfloat
#define VEC2 vk::Format::eR32G32Sfloat
#define VEC3 vk::Format::eR32G32B32Sfloat
#define VEC4 vk::Format::eR32G32B32A32Sfloat

// -------------- Vertex setup --------------
struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;
  glm::vec3 normal;

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
  }

  static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
    return {{
      {.location = 0, .binding = 0, .format = VEC3, .offset = offsetof(Vertex, pos)}, // attribute descriptions for position
      {.location = 1, .binding = 0, .format = VEC3, .offset = offsetof(Vertex, color)}, // attribute descriptions for colors
      {.location = 2, .binding = 0, .format = VEC2, .offset = offsetof(Vertex, texCoord)},
      {.location = 3, .binding = 0, .format = VEC3, .offset = offsetof(Vertex, normal)}
    }}; 
  }

  bool operator==(const Vertex& other) const {
      return pos == other.pos && color == other.color && texCoord == other.texCoord;
  }
};
namespace std {
  template<> struct hash<Vertex> {
    size_t operator()(Vertex const& vertex) const {
      return (
          (hash<glm::vec3>()(vertex.pos) ^
          (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
          (hash<glm::vec2>()(vertex.texCoord) << 1);// ^
          (hash<glm::vec3>()(vertex.normal) << 1);
    }
  };
}

//---------------- GameObject setup ----------------
struct GameObject {
  virtual ~GameObject() = default;

  std::string name = "unnamed";

  // transform properties
  glm::vec3 position = {0.0f, 0.0f, 0.0f};
  glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
  glm::vec3 scale = {1.0f, 1.0f, 1.0f};

  // stroes object geometry
  std::vector<Vertex>   vertices;
  std::vector<uint32_t> indices;

  vk::raii::Buffer       vertexBuffer       = nullptr;
  vk::raii::DeviceMemory vertexBufferMemory = nullptr;
  vk::raii::Buffer       indexBuffer        = nullptr;
  vk::raii::DeviceMemory indexBufferMemory  = nullptr;

  //stroes object textures
  vk::raii::Image        textureImage       = nullptr;
  vk::raii::DeviceMemory textureImageMemory = nullptr;
  vk::raii::ImageView    textureImageView   = nullptr;

  // Uniform buffer for the object
  std::vector<vk::raii::Buffer> uniformBuffers;
  std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;

  // descriptor set for object
  std::vector<vk::raii::DescriptorSet> descriptorSets;

  // calculates model matrix
  virtual glm::mat4 getModelMatrix() const {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
  }

  vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;

  uint32_t lighting = 2;
};
#endif
