// ---------- include statements -----------
#define VULKAN_HPP_NO_CONSTRUCTORS   // use designated initializers instead of vk:: constructors
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS 1   // add this
#include <vulkan/vulkan_raii.hpp>    // RAII wrappers for Vulkan handles (auto cleanup)

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <unordered_map>
#include <cmath>


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "core/stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "core/tiny_obj_loader.h"


#include "core/gameObject.h"
#include "core/GameObjects/plane.h"
#include "core/light.h"
#include "core/camera.h"

// 0 = fill, 1 = wireframe, 2 = vertex only
#define RENDERMODE 0 

// float format for vector attribute descriptions
#define VEC1 vk::Format::eR32Sfloat
#define VEC2 vk::Format::eR32G32Sfloat
#define VEC3 vk::Format::eR32G32B32Sfloat
#define VEC4 vk::Format::eR32G32B32A32Sfloat

#define PHONG 2
#define DIFFUSE 1
#define NONE 0

//------------ global constants -------------
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// Vulkan layers to enable when validation is on (extra error/usage checking from the SDK)
const std::vector<char const*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

// validation layers only get enabled in debug builds
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;  // number of frames that can be queued for rendering at once

constexpr int MAX_OBJECTS = 10;
constexpr int MAX_LIGHTS = 3;

constexpr uint32_t SHADOW_RESOLUTION = 16;
constexpr uint32_t SHADOW_MAP_SIZE = 1024 * SHADOW_RESOLUTION;


// --------------- Uniform setup ---------------
struct UniformBufferObject {
  // alignas(4) = scalar | alignas(8) = vector | alignas(16) = matrix or nested struct
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
  alignas(16) glm::mat4 lightSpaceMatrix;
  alignas(8)  glm::vec3 viewPos;
  alignas(16) std::array<Light, MAX_LIGHTS> lights;
  alignas(4)  int objLighting;

};



//---------------- Application class --------------
// wraps window creation, Vulkan setup, the render loop and teardown
class Application {
public:
  // entry point: sets up the window/Vulkan state, runs the render loop, then cleans up
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  //------------ member variables -------------
  GLFWwindow* window = nullptr;

  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;

  vk::raii::PhysicalDevice vulkanPhysicalDevice = nullptr;
  vk::raii::Device device = nullptr;

  vk::raii::Queue graphicsQueue = nullptr;

  vk::raii::SurfaceKHR surface = nullptr;

  vk::raii::SwapchainKHR swapChain = nullptr;
  std::vector<vk::Image> swapChainImages;
  vk::SurfaceFormatKHR swapChainSurfaceFormat;
  vk::Extent2D swapChainExtent;

  std::vector<vk::raii::ImageView> swapChainImageViews;

  uint32_t queueIndex = ~0u;  // index of the queue family used for graphics + present

  std::vector<const char*> requiredDeviceExtension = {
    vk::KHRSwapchainExtensionName
  };

  vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
  vk::raii::PipelineLayout pipelineLayout = nullptr;

  vk::raii::Pipeline graphicsPipeline = nullptr;

  vk::raii::CommandPool commandPool = nullptr;
  std::vector<vk::raii::CommandBuffer> commandBuffers;  // one per in-flight frame

  // per-frame sync: presentCompleteSemaphores/inFlightFences are sized MAX_FRAMES_IN_FLIGHT,
  // renderFinishedSemaphores is sized per swapchain image
  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  std::vector<vk::raii::Fence> inFlightFences;

  uint32_t frameIndex = 0;  // cycles 0..MAX_FRAMES_IN_FLIGHT-1
 
  bool framebufferResized = false;

  vk::raii::DescriptorPool descriptorPool = nullptr;
  
  uint32_t mipLevels = 0;
  vk::raii::Sampler textureSampler = nullptr;

  vk::raii::Image depthImage = nullptr;
  vk::raii::DeviceMemory depthImageMemory = nullptr;
  vk::raii::ImageView depthImageView =nullptr;

  vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e1;

  vk::raii::Image colorImage = nullptr;
  vk::raii::DeviceMemory colorImageMemory = nullptr;
  vk::raii::ImageView colorImageView = nullptr;

  vk::raii::Image shadowImage = nullptr;
  vk::raii::DeviceMemory shadowImageMemory = nullptr;
  vk::raii::ImageView shadowImageView = nullptr;
  vk::raii::Sampler shadowSampler = nullptr;
  vk::raii::PipelineLayout shadowPipelineLayout = nullptr;
  vk::raii::Pipeline shadowPipeline = nullptr;
  glm::mat4 lightSpaceMatrix{1.0f};




  // array for game objects
  std::array<std::unique_ptr<GameObject>, MAX_OBJECTS> gameObjects;

  std::array<Light, MAX_LIGHTS> lights;





  float deltaTime = 0.0f;
  float lastFrame = 0.0f;

  Camera camera{FIRSTPERSON, glm::vec3(0.0f, 0.5f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f)};

  float camAngle = 0.0f;



  void processInputs(GLFWwindow *window, Plane* plane) {
    // camera rotation
    float camSpeed = 180.0f;
    float step = camSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS){
      camAngle = std::max(camAngle - step, -90.0f);
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS){
      camAngle = std::min(camAngle + step, 90.0f);
    }
    else {
      if (camAngle > 0.0f) {
        camAngle = std::max(camAngle - step, 0.0f);
      }
      else if (camAngle < 0.0f) {
        camAngle = std::min(camAngle + step, 0.0f);
      }
    }

    // plane movement
    if (!plane->isCrashed){
      plane->planePitch = 0.0f;
      plane->planeYaw = 0.0f;
      plane->planeRoll = 0.0f;

      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS){
        plane->planePitch -= 1 * deltaTime;
      }
      if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){
        plane->planePitch += 1 * deltaTime;
      }
      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS){
        plane->planeRoll += 2 * deltaTime;
      }
      if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS){
        plane->planeRoll -= 2 * deltaTime;
      }
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS){
        plane->planeYaw += 1 * deltaTime;
      }
      if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS){
        plane->planeYaw -= 1 * deltaTime;
      }

      if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS){
        if(plane->planeSpeed < plane->maxSpeed){
          plane->planeSpeed += 1.5f * deltaTime;
        }
        else{
          plane->planeSpeed = plane->maxSpeed;
        }
      }
      if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS){
        if(plane->planeSpeed > 0.0f){
          plane->planeSpeed -= 1.5f * deltaTime;
        }
        else{
          plane->planeSpeed = 0.0f;
        }
      }
    }

    // debug inputs
    if(glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS){
      std::cout<<camera.position.x<<" "<<camera.position.y<<" "<<camera.position.z<<" | "<<gameObjects[0]->position.x<<" "<<gameObjects[0]->position.y<<gameObjects[0]->position.z<<std::endl;
    }

  }

  // creates a fixed-size GLFW window with no OpenGL context (Vulkan handles rendering)
  void initWindow() {
    std::cout << "init window " << std::endl;
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  }

  static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
  }

  // brings up the full Vulkan pipeline, in dependency order
  void initVulkan() {
    std::cout << "init Vulkan" << std::endl;
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createDescriptorLayout();
    createShadowResources();
    createShadowPipeline();
    createGraphicsPipeline();
    createCommandPool();
    createColorResources();
    createDepthResources();
    createTextureSampler();
    setupGameObjects();
    setupLights();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();


    
  }

  // polls window events and draws until the window is closed, then waits for the GPU to finish
  void mainLoop() {
    std::cout << "starting mainloop" << std::endl;

    Plane* plane = static_cast<Plane*>(gameObjects[0].get());
    GameObject* ground = static_cast<GameObject*>(gameObjects[1].get());
    GameObject* skybox = static_cast<GameObject*>(gameObjects[2].get());

    while (!glfwWindowShouldClose(window)) {
      // handles delta time for consistent animations / movements regardless of framerate
      float currentTime = glfwGetTime();
      deltaTime = currentTime - lastFrame;
      lastFrame = currentTime;
      
      processInputs(window, plane);

      glm::mat4 deltaRotation(1.0f);
      deltaRotation = glm::rotate(deltaRotation, plane->planePitch, glm::vec3(1.0f, 0.0f, 0.0f));
      deltaRotation = glm::rotate(deltaRotation, plane->planeYaw,   glm::vec3(0.0f, 1.0f, 0.0f));
      deltaRotation = glm::rotate(deltaRotation, plane->planeRoll,  glm::vec3(0.0f, 0.0f, 1.0f));
      plane->planeOrientation = plane->planeOrientation * deltaRotation;


      glm::vec3 forward = glm::vec3(plane->planeOrientation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));

      plane->position += forward * plane->planeSpeed * deltaTime;

      lights[1].position = plane->position - glm::vec3{0.0f, 0.0f, 1.0f};
      lights[2].position = plane->position + glm::vec3{0.0f, 0.0f, 1.0f};
      
      if (plane->position.y <= 0.0f){
        plane->crash();
      }
      

      skybox->position = camera.position;
      camera.followObject(plane, forward, 5.0f, camAngle, deltaTime);


      glfwPollEvents();
      drawFrame();
    }
    device.waitIdle();
  }

  // tears down window/swapchain resources not already handled by the RAII Vulkan wrappers
  void cleanup() {
    std::cout << "cleanup" << std::endl;

    swapChainImageViews.clear();
    swapChain = nullptr;
    surface = nullptr;

    glfwDestroyWindow(window);
    glfwTerminate();
  }

  // -------- Helper Methods -------

	void recreateSwapChain() {
		int width = 0, height = 0;
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		device.waitIdle();

		cleanupSwapChain();
		createSwapChain();
		createImageViews();
    createColorResources();
		createDepthResources();
	}

  void cleanupSwapChain() {
    depthImageView = nullptr;
    depthImage = nullptr;
    depthImageMemory = nullptr;
    swapChainImageViews.clear();
    swapChain = nullptr;
  }

  // creates the Vulkan instance after confirming the requested layers/extensions are available
  void createInstance() {
    constexpr vk::ApplicationInfo appinfo{
      .pApplicationName = "Hello Triangle",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = vk::ApiVersion14
    };

    // Get the required layers
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers) {
      requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if the required layers are supported by the Vulkan implementation.
    auto layerProperties = context.enumerateInstanceLayerProperties();

    auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
      [&layerProperties](auto const& requiredLayer) {
        return std::ranges::none_of(layerProperties,
          [requiredLayer](auto const& layerProperty) {
            return strcmp(layerProperty.layerName, requiredLayer) == 0;
          });
      });
    if (unsupportedLayerIt != requiredLayers.end()) {
      throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
    }

    auto requiredExtensions = getRequiredInstanceExtensions();

    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    auto unsupportedPropertyIt = std::ranges::find_if(requiredExtensions,
      [&extensionProperties](auto const& requiredExtension) {
        return std::ranges::none_of(extensionProperties,
          [requiredExtension](auto const& extensionProperty) {
            return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
          });
      });
    if (unsupportedPropertyIt != requiredExtensions.end()) {
      throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
    }

    vk::InstanceCreateInfo createInfo{
      .pApplicationInfo = &appinfo,
      .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
      .ppEnabledLayerNames = requiredLayers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
      .ppEnabledExtensionNames = requiredExtensions.data()
    };

    instance = vk::raii::Instance(context, createInfo);
  }

  // asks GLFW which instance extensions it needs for presenting to a window on this platform
  std::vector<const char*> getRequiredInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    return extensions;
  }

  // filters available GPUs down to ones that meet our requirements, then lets the user pick one
  void pickPhysicalDevice() {
    auto physicalDevices = instance.enumeratePhysicalDevices();

    if (physicalDevices.empty()) {
      throw std::runtime_error("No physical device with vulkan support");
    }

    std::vector<vk::raii::PhysicalDevice> compatibleDevices;
    for (auto physicalDevice : physicalDevices) {
      auto properties = physicalDevice.getProperties();

      // needs Vulkan 1.3+ for dynamic rendering / synchronization2
      bool versionCheck = properties.apiVersion >= vk::ApiVersion13;

      // needs at least one queue family that supports graphics commands
      auto queueFamilies = physicalDevice.getQueueFamilyProperties();
      bool queueCheck = std::ranges::any_of(queueFamilies,
        [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

      // needs swapchain support to be able to present to the window
      std::vector<const char*> requiredDeviceExtension = { vk::KHRSwapchainExtensionName };
      auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
      bool extensionCheck = std::ranges::all_of(requiredDeviceExtension,
        [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
          return std::ranges::any_of(availableDeviceExtensions,
            [requiredDeviceExtension](auto const& availableDeviceExtension) {
              return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
            });
        });

      // needs the specific device features this app relies on (dynamic rendering, wireframe fill, etc)
      auto features = physicalDevice.template getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
      bool featureCheck =
        features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
        features.template get<vk::PhysicalDeviceFeatures2>().features.fillModeNonSolid &&
        features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy;

      if (versionCheck && queueCheck && extensionCheck && featureCheck) {
        compatibleDevices.push_back(physicalDevice);
      }
    }

    // print compatible GPUs and let the user choose which one to use
    for (size_t i = 0; i < compatibleDevices.size(); i++) {
      std::cout << i << ". " << compatibleDevices.at(i).getProperties().deviceName << std::endl;
    }

    int choice;
    std::cout << "Select GPU number: ";
    std::cin >> choice;
    vulkanPhysicalDevice = compatibleDevices.at(choice);
    std::cout << compatibleDevices.at(choice).getProperties().deviceName << " selected" << std::endl;

    msaaSamples = getMaxUsableSampleCount(); // gets max MSAA sample count for selected gpu
  }

  // finds a queue family that supports both graphics and present, then creates the logical device
  void createLogicalDevice() {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = vulkanPhysicalDevice.getQueueFamilyProperties();

    // look for a single queue family that can do both graphics and presenting to our surface
    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
      if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
          vulkanPhysicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
        queueIndex = qfpIndex;
        break;
      }
    }
    if (queueIndex == ~0u) {
      throw std::runtime_error("Could not find queue for graphics and present");
    }

    // chain together the feature structs we need enabled on the device (must match pickPhysicalDevice's checks)
    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                        vk::PhysicalDeviceVulkan11Features,
                        vk::PhysicalDeviceVulkan13Features,
                        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain = {
        { .features = { .fillModeNonSolid = vk::True, .samplerAnisotropy = true } },  // enable optional features
        { .shaderDrawParameters = true },
        { .synchronization2 = true, .dynamicRendering = true },
        { .extendedDynamicState = true }
      };

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
      .queueFamilyIndex = queueIndex,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority
    };
    vk::DeviceCreateInfo deviceCreateInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &deviceQueueCreateInfo,
      .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
      .ppEnabledExtensionNames = requiredDeviceExtension.data()
    };

    device = vk::raii::Device(vulkanPhysicalDevice, deviceCreateInfo);
    graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
  }

  // wraps the GLFW-created window surface in a vk::raii::SurfaceKHR
  void createSurface() {
    VkSurfaceKHR tempSurface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &tempSurface) != 0) {
      throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, tempSurface);
  }

  // prefers sRGB 8-bit BGRA, falls back to whatever format is available first
  vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    const auto formatIt = std::ranges::find_if(availableFormats,
      [](const auto& format) {
        return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      });
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
  }

  // prefers mailbox (low-latency triple buffering), falls back to fifo (guaranteed vsync)
  vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
    return std::ranges::any_of(availablePresentModes,
      [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; })
      ? vk::PresentModeKHR::eMailbox
      : vk::PresentModeKHR::eFifo;
  }

  // uses the surface's current extent if defined, otherwise derives it from the framebuffer size
  vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
      std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
      std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
  }

  // requests at least 3 swapchain images, clamped to the surface's supported maximum
  uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities) {
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
      minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
  }

  // creates the swapchain using the format/present-mode/extent chosen above, and grabs its images
  void createSwapChain() {
    auto surfaceCapabilities = vulkanPhysicalDevice.getSurfaceCapabilitiesKHR(*surface);
    swapChainExtent = chooseSwapExtent(surfaceCapabilities);

    uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

    auto availableFormats = vulkanPhysicalDevice.getSurfaceFormatsKHR(*surface);
    swapChainSurfaceFormat = chooseSurfaceFormat(availableFormats);

    auto availablePresentModes = vulkanPhysicalDevice.getSurfacePresentModesKHR(*surface);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{
      .surface = *surface,
      .minImageCount = minImageCount,
      .imageFormat = swapChainSurfaceFormat.format,
      .imageColorSpace = swapChainSurfaceFormat.colorSpace,
      .imageExtent = swapChainExtent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surfaceCapabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = chooseSwapPresentMode(availablePresentModes),
      .clipped = true,
      .oldSwapchain = nullptr
    };

    swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();
  }

  // creates one 2D color image view per swapchain image, for use as render targets
  void createImageViews() {
    assert(swapChainImageViews.empty());

    swapChainImageViews.reserve(swapChainImages.size());
    for (auto& image : swapChainImages) {
      swapChainImageViews.emplace_back(createImageView(image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor, 1));
    }
  }

  // builds the fixed-function + shader stages of the graphics pipeline used to draw the triangle.
  // uses dynamic rendering (no vk::RenderPass) and dynamic viewport/scissor.
  void createGraphicsPipeline() {
    //----- shader stage setup -----
    std::string dir = "src/shaders/SPIR-V/";
    auto vertexCode = readFile(dir + "vertex.spv");
    auto fragmentCode = readFile(dir + "fragment.spv");

    vk::raii::ShaderModule vertexShaderModule = createShaderModule(vertexCode);
    vk::raii::ShaderModule fragmentShaderModule = createShaderModule(fragmentCode);

    vk::PipelineShaderStageCreateInfo vertexShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = vertexShaderModule,
      .pName = "main"
    };

    vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = fragmentShaderModule,
      .pName = "main"
    };

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, fragmentShaderStageInfo };

    // no vertex buffers - the vertex shader generates the triangle's positions itself
    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount   = 1,
      .pVertexBindingDescriptions      = &bindingDescription,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
      .pVertexAttributeDescriptions    = attributeDescriptions.data()
    };

    //----- viewport / scissor / dynamic state -----
    // initial values here don't matter much since viewport & scissor are set dynamically per-frame
    vk::Viewport viewport{
      0.0f, 0.0f,
      static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height),
      0.0f, 1.0f
    };
    vk::Rect2D scissor{ vk::Offset2D{ 0, 0 }, swapChainExtent };

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eCullMode};
    vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()
    };

    vk::PipelineViewportStateCreateInfo viewportState{
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor
    };

    //----- rasterization / multisampling / blending -----
    // RENDERMODE selects fill/wireframe/point rendering (see top of file)
    vk::PolygonMode renderModes[] = { vk::PolygonMode::eFill, vk::PolygonMode::eLine, vk::PolygonMode::ePoint };
    vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = renderModes[RENDERMODE],
      .cullMode = vk::CullModeFlagBits::eNone,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = vk::False,
      .lineWidth = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = msaaSamples};

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False
    };

    // standard alpha blending: srcAlpha * src + (1 - srcAlpha) * dst
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = vk::True,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                         vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment
    };

    // sets layout to use uniform buffer
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
      .setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0 
    };
    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    vk::Format depthFormat = findDepthFormat();

    //----- assemble and create the pipeline -----
    // PipelineRenderingCreateInfo is chained in since we're using dynamic rendering (no render pass object)
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
      {
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = nullptr
      },
      {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapChainSurfaceFormat.format, .depthAttachmentFormat = depthFormat
      }
    };

    graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
  }

  // eResetCommandBuffer allows individual command buffers to be re-recorded each frame
  void createCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueIndex
    };
    commandPool = vk::raii::CommandPool(device, poolInfo);
  }

  // allocates one primary command buffer per in-flight frame
  void createCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };
    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
  }

  // records the draw commands for one frame into the command buffer for the given swapchain image
  void recordCommandBuffer(uint32_t imageIndex) {
    commandBuffers[frameIndex].begin({});

    // ---------- shadow pass ----------
    transition_image_layout(
      *shadowImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
      {}, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::ImageAspectFlagBits::eDepth
    );

    vk::ClearValue shadowClear{ .depthStencil = vk::ClearDepthStencilValue(1.0f, 0) };
    vk::RenderingAttachmentInfo shadowAttachment{
      .imageView = shadowImageView,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = shadowClear
    };
    vk::RenderingInfo shadowRenderingInfo{
      .renderArea = { .offset = {0, 0}, .extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE} },
      .layerCount = 1,
      .colorAttachmentCount = 0,
      .pDepthAttachment = &shadowAttachment
    };

    commandBuffers[frameIndex].beginRendering(shadowRenderingInfo);
    commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *shadowPipeline);
    commandBuffers[frameIndex].setViewport(0, vk::Viewport(0.0f, 0.0f, (float)SHADOW_MAP_SIZE, (float)SHADOW_MAP_SIZE, 0.0f, 1.0f));
    commandBuffers[frameIndex].setScissor(0, vk::Rect2D({0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}));

    for (const auto& gameObjectPTR : gameObjects) {
      auto& gameObject = *gameObjectPTR;
      commandBuffers[frameIndex].bindVertexBuffers(0, *gameObject.vertexBuffer, {0});
      commandBuffers[frameIndex].bindIndexBuffer(*gameObject.indexBuffer, 0, vk::IndexType::eUint32);

      struct { glm::mat4 model; glm::mat4 lightSpace; } pushData{
        gameObject.getModelMatrix(), lightSpaceMatrix
      };
      commandBuffers[frameIndex].pushConstants<decltype(pushData)>(
        *shadowPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, pushData
      );
      commandBuffers[frameIndex].drawIndexed(gameObject.indices.size(), 1, 0, 0, 0);
    }
    commandBuffers[frameIndex].endRendering();

    transition_image_layout(
      *shadowImage, vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eShaderRead,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::PipelineStageFlagBits2::eFragmentShader,
      vk::ImageAspectFlagBits::eDepth
    );

    // ---------- main render pass ----------
    // undefined -> color attachment, so the GPU can render into this swapchain image
    transition_image_layout(
      swapChainImages[imageIndex],
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      {},
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::ImageAspectFlagBits::eColor
    );

    transition_image_layout(
      *colorImage,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::ImageAspectFlagBits::eColor
    );

    transition_image_layout(
      *depthImage,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eDepthAttachmentOptimal,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::ImageAspectFlagBits::eDepth
    );


    // dynamic rendering: describe the color attachment directly, no render pass/framebuffer objects needed
    vk::ClearValue clearColor{ .color = vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } } };
    vk::ClearValue clearDepth{.depthStencil = vk::ClearDepthStencilValue(1.0f, 0)};

    vk::RenderingAttachmentInfo colorAttachmentInfo = {
        .imageView          = colorImageView,
        .imageLayout        = vk::ImageLayout::eColorAttachmentOptimal,
        .resolveMode        = vk::ResolveModeFlagBits::eAverage,
        .resolveImageView   = swapChainImageViews[imageIndex],
        .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp             = vk::AttachmentLoadOp::eClear,
        .storeOp            = vk::AttachmentStoreOp::eStore,
        .clearValue         = clearColor
    };

    vk::RenderingAttachmentInfo depthAttachmentInfo = {
      .imageView = depthImageView,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = clearDepth
    };

    vk::RenderingInfo renderingInfo = {
      .renderArea = { .offset = { 0, 0 }, .extent = swapChainExtent },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentInfo,
      .pDepthAttachment = &depthAttachmentInfo
    };

    // bind the pipeline, set the dynamic viewport/scissor, then draw the hardcoded 3-vertex triangle
    commandBuffers[frameIndex].beginRendering(renderingInfo);
    commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
    commandBuffers[frameIndex].setViewport(0, vk::Viewport(
      0.0f, 0.0f,
      static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height),
      0.0f, 1.0f));
    commandBuffers[frameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));


    // Draw each object with its own descriptor set
    for (const auto& gameObjectPTR : gameObjects) {
      auto& gameObject = *gameObjectPTR;

      commandBuffers[frameIndex].setCullMode(gameObject.cullMode);

      commandBuffers[frameIndex].bindVertexBuffers(0, *gameObject.vertexBuffer, {0});
      commandBuffers[frameIndex].bindIndexBuffer(*gameObject.indexBuffer, 0, vk::IndexType::eUint32);

      // Bind the descriptor set for this object
      commandBuffers[frameIndex].bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *pipelineLayout,
        0,
        *gameObject.descriptorSets[frameIndex],
        nullptr
      );

      // Draw the object
      commandBuffers[frameIndex].drawIndexed(gameObject.indices.size(), 1, 0, 0, 0);
    }
    commandBuffers[frameIndex].endRendering();

    // color attachment -> present, so the swapchain can present this image to the screen
    transition_image_layout(
      swapChainImages[imageIndex],
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      {},
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::PipelineStageFlagBits2::eBottomOfPipe,
      vk::ImageAspectFlagBits::eColor
    );


    commandBuffers[frameIndex].end();
  }

  // reads a whole binary file (e.g. a .spv shader) into a byte buffer
  static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error("failed to open file!");
    }

    std::vector<char> buffer(file.tellg());

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();
    return buffer;
  }

  // wraps SPIR-V bytecode in a vk::raii::ShaderModule
  [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };
    vk::raii::ShaderModule shaderModule{ device, createInfo };
    return shaderModule;
  }

  // records an image memory barrier that transitions a swapchain image between layouts
  // (e.g. undefined -> color attachment, or color attachment -> present)
  void transition_image_layout(
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access_mask,
    vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags image_aspect_flags) {

      vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
          .aspectMask = image_aspect_flags,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1
      }
    };
    vk::DependencyInfo dependency_info = {
      .dependencyFlags = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier
    };

    commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
  }

  // creates the semaphores/fences used to synchronize the CPU, GPU and presentation engine
  void createSyncObjects() {
    assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

    // one "render finished" semaphore per swapchain image (signaled by the queue, waited on by present)
    for (size_t i = 0; i < swapChainImages.size(); i++) {
      renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    }

    // one "image acquired" semaphore and in-flight fence per frame-in-flight slot;
    // fences start signaled so the first frame doesn't block waiting on them
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
      inFlightFences.emplace_back(device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
    }
  }

  // renders and presents a single frame: wait -> acquire -> record -> submit -> present
  void drawFrame() {
    // wait until the GPU has finished with this frame-in-flight slot from MAX_FRAMES_IN_FLIGHT frames ago
    auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess) {
      throw std::runtime_error("failed to wait for fence!");
    }

    // acquire the next available swapchain image; signals presentCompleteSemaphores[frameIndex] when ready
		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

		// Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
		// here and does not need to be caught by an exception.
		if (result == vk::Result::eErrorOutOfDateKHR) {
			recreateSwapChain();
			return;
		}
		// On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
		// On any error code, aquireNextImage already threw an exception.
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
			assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
			throw std::runtime_error("failed to acquire swap chain image!");
		}

    device.resetFences(*inFlightFences[frameIndex]);

    recordCommandBuffer(imageIndex);
    
    updateUniformBuffers(frameIndex);
    // submit: wait for the image to be acquired before writing color output, signal when rendering is done
    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo submitInfo{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*presentCompleteSemaphores[frameIndex],
      .pWaitDstStageMask = &waitDestinationStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffers[frameIndex],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex]
    };

    graphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);

    // present: wait for rendering to finish before showing the image on screen
    const vk::PresentInfoKHR presentInfoKHR{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
      .swapchainCount = 1,
      .pSwapchains = &*swapChain,
      .pImageIndices = &imageIndex
    };

    result = graphicsQueue.presentKHR(presentInfoKHR);
		// Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
		// here and does not need to be caught by an exception.
		if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		}
		else {
			// There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
			assert(result == vk::Result::eSuccess);
		}

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
  }
  
  // creates buffers to send vertex data to gpu
  void createVertexBuffer(GameObject& obj) {
    vk::DeviceSize bufferSize = sizeof(obj.vertices[0]) * obj.vertices.size();

    auto [stagingBuffer, stagingBufferMemory] = createBuffer(
      bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );

    void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(dataStaging, obj.vertices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();

    std::tie(obj.vertexBuffer, obj.vertexBufferMemory) = createBuffer(
      bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal);

    copyBuffer(stagingBuffer, obj.vertexBuffer, bufferSize);
  }

  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties = vulkanPhysicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }

    throw std::runtime_error("failed to find suitable memory type");
  }

  std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(
      vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    vk::BufferCreateInfo bufferInfo{.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
    vk::raii::Buffer buffer = vk::raii::Buffer(device, bufferInfo);
    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
    vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
    buffer.bindMemory(*bufferMemory, 0);
    return {std::move(buffer), std::move(bufferMemory)};
  }

  void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) {
    vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
    commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{.size = size});
    endSingleTimeCommands(std::move(commandCopyBuffer));
  }

  void createIndexBuffer(GameObject& obj) {
      vk::DeviceSize bufferSize = sizeof(obj.indices[0]) * obj.indices.size();

      auto [stagingBuffer, stagingBufferMemory] =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

      void *data = stagingBufferMemory.mapMemory(0, bufferSize);
      memcpy(data, obj.indices.data(), (size_t) bufferSize);
      stagingBufferMemory.unmapMemory();

      std::tie(obj.indexBuffer, obj.indexBufferMemory) =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

      copyBuffer(stagingBuffer, obj.indexBuffer, bufferSize);
  }

  void createUniformBuffers() {
    // For each game object
    for (auto& gameObjectPTR : gameObjects) {
      auto& gameObject = *gameObjectPTR;
      gameObject.uniformBuffers.clear();
      gameObject.uniformBuffersMemory.clear();
      gameObject.uniformBuffersMapped.clear();

      // Create uniform buffers for each frame in flight
      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
        auto[buffer, bufferMem] = createBuffer(
          bufferSize,
          vk::BufferUsageFlagBits::eUniformBuffer,
          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        gameObject.uniformBuffers.emplace_back(std::move(buffer));
        gameObject.uniformBuffersMemory.emplace_back(std::move(bufferMem));
        gameObject.uniformBuffersMapped.emplace_back(gameObject.uniformBuffersMemory[i].mapMemory(0, bufferSize));
      }
    }
  }

  void updateUniformBuffers(uint32_t currentImage) {

    glm::mat4 view = camera.getViewMatrix();

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 1000.0f);
    projection[1][1] *= -1;
    
    glm::vec3 viewPos = camera.position;

    lightSpaceMatrix = createLightSpaceMatrix();

    // Update uniform buffers for each object
    for (auto& gameObjectPTR : gameObjects) {
      auto& gameObject = *gameObjectPTR;

      // Get the model matrix for this object
      glm::mat4 modelMat = gameObject.getModelMatrix();

      // Create and update the UBO
      UniformBufferObject ubo{
        .model = modelMat,
        .view = view,
        .projection = projection,
        .lightSpaceMatrix = lightSpaceMatrix,
        .viewPos = viewPos,
        .objLighting = gameObject.lighting
      };

      for (size_t i = 0; i < MAX_LIGHTS; i++) {
        ubo.lights[i].position = lights[i].position;
        ubo.lights[i].color    = lights[i].color;
      }

      // Copy the UBO data to the mapped memory
      memcpy(gameObject.uniformBuffersMapped[frameIndex], &ubo, sizeof(ubo));
    }
  }

  void createDescriptorLayout() {
    std::array<vk::DescriptorSetLayoutBinding, 3> uboLayoutBindings{{
      {.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment},
      {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment},
      {.binding = 2, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}
    }};
    vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .bindingCount = static_cast<uint32_t>(uboLayoutBindings.size()),
      .pBindings = uboLayoutBindings.data()
    };
    descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
  }

  void createDescriptorPool() {
    // We need MAX_OBJECTS * MAX_FRAMES_IN_FLIGHT descriptor sets
    std::array poolSize {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_OBJECTS * MAX_FRAMES_IN_FLIGHT),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 2 * MAX_OBJECTS * MAX_FRAMES_IN_FLIGHT)
    };
    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = MAX_OBJECTS * MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
        .pPoolSizes = poolSize.data()
    };
    descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
  }

  void createDescriptorSets() {
    // For each game object
    for (auto& gameObjectPTR : gameObjects) {
      auto& gameObject = *gameObjectPTR;

      // Create descriptor sets for each frame in flight
      std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
      vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()
      };

      gameObject.descriptorSets.clear();
      gameObject.descriptorSets = device.allocateDescriptorSets(allocInfo);

      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo bufferInfo{
          .buffer = *gameObject.uniformBuffers[i],
          .offset = 0,
          .range = sizeof(UniformBufferObject)
        };
        vk::DescriptorImageInfo imageInfo{
          .sampler = *textureSampler,
          .imageView = *gameObject.textureImageView,
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };
        vk::DescriptorImageInfo shadowInfo{
          .sampler = *shadowSampler,
          .imageView = *shadowImageView,
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        std::array descriptorWrites{
          vk::WriteDescriptorSet{
            .dstSet = *gameObject.descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo
          },
          vk::WriteDescriptorSet{
            .dstSet = *gameObject.descriptorSets[i],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo
          },
          vk::WriteDescriptorSet{
            .dstSet = *gameObject.descriptorSets[i],
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &shadowInfo
          }

        };
        device.updateDescriptorSets(descriptorWrites, {});
      }
    }
  }

  void createTextureImage(GameObject& obj, std::string path) {
    int texWidth, texHeight, texChannels;

    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;


    if (!pixels) {
      throw std::runtime_error("failed to load texture! " + path);
    }

    auto [stagingBuffer, stagingBufferMemory] = createBuffer(
      imageSize,
      vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    std::tie(obj.textureImage, obj.textureImageMemory) = createImage(
      texWidth,
      texHeight,
      mipLevels,
      vk::SampleCountFlagBits::e1,
      vk::Format::eR8G8B8A8Srgb,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
    transitionImageLayout(commandBuffer, obj.textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
    copyBufferToImage(commandBuffer, stagingBuffer, obj.textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    generateMipmaps(commandBuffer, obj.textureImage, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels);
    endSingleTimeCommands(std::move(commandBuffer));
  }

  std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) {
    vk::ImageCreateInfo imageInfo{
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent = {width, height, 1},
      .mipLevels = mipLevels,
      .arrayLayers = 1,
      .samples = numSamples,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive
    };

    vk::raii::Image image = vk::raii::Image(device, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
    };
    vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
    image.bindMemory(imageMemory, 0);

    return {std::move(image), std::move(imageMemory)};
  }

  vk::raii::CommandBuffer beginSingleTimeCommands() {
    vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1
    };

    vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    commandBuffer.begin(beginInfo);

    return std::move(commandBuffer);
  }

  void endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo{
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffer
    };
    graphicsQueue.submit(submitInfo, nullptr);
    graphicsQueue.waitIdle();
  }

  void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels) {
    vk::ImageMemoryBarrier barrier{
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = image,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = mipLevels, .layerCount = 1}
    };

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
      barrier.srcAccessMask = {};
      barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

      sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
      destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

      sourceStage      = vk::PipelineStageFlagBits::eTransfer;
      destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else {
      throw std::invalid_argument("unsupported layout transition!");
    }

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
  }

  void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height) {
    vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1}
    };

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
  }

  void createTextureImageView(GameObject& obj) {
    obj.textureImageView = createImageView(*obj.textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);
  }

  vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags, int32_t mipLevels) {
    vk::ImageViewCreateInfo viewInfo{
      .image = image,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1}
    };

    return vk::raii::ImageView(device, viewInfo);

  }

  void createTextureSampler() {
    vk::PhysicalDeviceProperties properties = vulkanPhysicalDevice.getProperties();
    // vk::Filter::eLinear for bilinear filtering, vk::Filter::eNearest for nearest neighbor sampling (pixelated)
    vk::SamplerCreateInfo samplerInfo{
      .magFilter = vk::Filter::eNearest,
      .minFilter = vk::Filter::eNearest,
      .mipmapMode = vk::SamplerMipmapMode::eNearest,
      .addressModeU = vk::SamplerAddressMode::eRepeat,
      .addressModeV = vk::SamplerAddressMode::eRepeat,
      .addressModeW = vk::SamplerAddressMode::eRepeat,
      .mipLodBias = 0.0f,
      .anisotropyEnable = vk::True,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways,
      .minLod = 0.0f,
      .maxLod = vk::LodClampNone,
      .borderColor = vk::BorderColor::eIntOpaqueBlack,
      .unnormalizedCoordinates = vk::False
    };

    textureSampler = vk::raii::Sampler(device, samplerInfo);
  }

  void createDepthResources() {
    vk::Format depthFormat = findDepthFormat();

    std::tie(depthImage, depthImageMemory) = createImage(
      swapChainExtent.width,
      swapChainExtent.height, 
      1,
      msaaSamples,
      depthFormat,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::MemoryPropertyFlagBits::eDeviceLocal
    );
    depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
  }

  vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (const auto format : candidates) {
      vk::FormatProperties props = vulkanPhysicalDevice.getFormatProperties(format);

      if (((tiling == vk::ImageTiling::eLinear)  && ((props.linearTilingFeatures & features) ==  features)) ||
          ((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features))) {
        return format;
      }
    }
    throw std::runtime_error("failed to find supported format!");
  }

  vk::Format findDepthFormat() {
    return findSupportedFormat(
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
  }

  void loadModel(GameObject& obj, std::string path) {
      tinyobj::attrib_t attrib;
      std::vector<tinyobj::shape_t> shapes;
      std::vector<tinyobj::material_t> materials;
      std::string warn, err;

      std::string baseDir = path.substr(0, path.find_last_of("/\\") + 1);

      if (!tinyobj::LoadObj(
          &attrib,
          &shapes,
          &materials,
          &warn,
          &err,
          path.c_str(),
          baseDir.c_str()
      )) {
          throw std::runtime_error(warn + err);
      }

      if (!warn.empty()) {
          std::cout << "TinyObj warning: " << warn << std::endl;
      }

      if (!err.empty()) {
          std::cout << "TinyObj error: " << err << std::endl;
      }

      std::unordered_map<Vertex, uint32_t> uniqueVertices{};

      for (const auto& shape : shapes) {

          for (size_t i = 0; i < shape.mesh.indices.size(); i++) {

              const auto& index = shape.mesh.indices[i];

              Vertex vertex{};

              vertex.pos = {
                  attrib.vertices[3 * index.vertex_index + 0],
                  attrib.vertices[3 * index.vertex_index + 1],
                  attrib.vertices[3 * index.vertex_index + 2]
              };

              if (index.texcoord_index >= 0) {
                  vertex.texCoord = {
                      attrib.texcoords[2 * index.texcoord_index + 0],
                      1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                  };
              }

              if (index.normal_index >= 0) {
                  vertex.normal = {
                      attrib.normals[3 * index.normal_index + 0],
                      attrib.normals[3 * index.normal_index + 1],
                      attrib.normals[3 * index.normal_index + 2]
                  };
              }
              else {
                  vertex.normal = {0.0f, 0.0f, 0.0f};
              }

              vertex.color = {1.0f, 1.0f, 1.0f};

              auto [it, inserted] =
                  uniqueVertices.insert({
                      vertex,
                      static_cast<uint32_t>(obj.vertices.size())
                  });

              if (inserted) {
                  obj.vertices.push_back(vertex);
              }

              obj.indices.push_back(it->second);
          }
      }

      if (materials.empty()) {
          std::cout << obj.name << " has no materials" << std::endl;
          return;
      }

      const auto& material = materials[0];

      if (!material.diffuse_texname.empty()) {

          std::string texturePath = baseDir + material.diffuse_texname;

          std::cout << obj.name
                    << " texture: "
                    << texturePath
                    << std::endl;

          createTextureImage(obj, texturePath);
          createTextureImageView(obj);
      }
      else {
          std::cout << obj.name
                    << " material has no diffuse texture"
                    << std::endl;
      }
  }

  void generateMipmaps(vk::raii::CommandBuffer &commandBuffer, vk::raii::Image &image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    vk::FormatProperties formatProperties = vulkanPhysicalDevice.getFormatProperties(imageFormat);
    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
      throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::ImageMemoryBarrier barrier = {
      .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
      .dstAccessMask = vk::AccessFlagBits::eTransferRead,
      .oldLayout = vk::ImageLayout::eTransferDstOptimal,
      .newLayout = vk::ImageLayout::eTransferSrcOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = image,
		  .subresourceRange    = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1}
    };

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
      barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

      commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

      vk::ImageBlit blit = {
        .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i - 1, .layerCount = 1},
        .srcOffsets     = std::array<vk::Offset3D, 2>({{}, {mipWidth, mipHeight, 1}}),
        .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i, .layerCount = 1},
        .dstOffsets     = std::array<vk::Offset3D, 2>({{}, {1 < mipWidth ? mipWidth / 2 : 1, 1 < mipHeight ? mipHeight / 2 : 1, 1}})
      };

      commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

      barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
      barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
      barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

      commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);


      if (1 < mipWidth) {
        mipWidth /= 2;
      }
      if (1 < mipHeight) {
        mipHeight /=2;
      }

    }
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier); 
  }

  vk::SampleCountFlagBits getMaxUsableSampleCount() {
    vk::PhysicalDeviceProperties physicalDeviceProperties = vulkanPhysicalDevice.getProperties();

    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
    if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
    if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
    if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

    return vk::SampleCountFlagBits::e1;
  }

  void createColorResources() {
    vk::Format colorFormat = swapChainSurfaceFormat.format;
    std::tie(colorImage, colorImageMemory) = createImage(
      swapChainExtent.width,
      swapChainExtent.height,
      1,
      msaaSamples,
      colorFormat,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
      vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    colorImageView = createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
  }

  void createShadowResources() {
    vk::Format depthFormat = findDepthFormat();
    std::tie(shadowImage, shadowImageMemory) = createImage(
        SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1,
        vk::SampleCountFlagBits::e1,
        depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal
      );
      shadowImageView = createImageView(shadowImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);

    vk::SamplerCreateInfo samplerInfo{
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eNearest,
      .addressModeU = vk::SamplerAddressMode::eClampToBorder,
      .addressModeV = vk::SamplerAddressMode::eClampToBorder,
      .addressModeW = vk::SamplerAddressMode::eClampToBorder,
      .anisotropyEnable = vk::False,
      .compareEnable = vk::True,
      .compareOp = vk::CompareOp::eLessOrEqual,
      .borderColor = vk::BorderColor::eFloatOpaqueWhite,  // outside frustum = "not in shadow"
      .unnormalizedCoordinates = vk::False
    };
    shadowSampler = vk::raii::Sampler(device, samplerInfo);
  }

  void createShadowPipeline() {
    auto vertexCode = readFile("src/shaders/SPIR-V/shadow.spv");
    vk::raii::ShaderModule shadowVertModule = createShaderModule(vertexCode);

    vk::PipelineShaderStageCreateInfo vertStage{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = shadowVertModule,
      .pName = "main"
    };

    std::vector<vk::DynamicState> shadowDynamicStates = {
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo shadowDynamicState{
      .dynamicStateCount = static_cast<uint32_t>(shadowDynamicStates.size()),
      .pDynamicStates = shadowDynamicStates.data()
    };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescription,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
      .pVertexAttributeDescriptions = attributeDescriptions.data()
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList
    };

    vk::Viewport viewport{0.0f, 0.0f, (float)SHADOW_MAP_SIZE, (float)SHADOW_MAP_SIZE, 0.0f, 1.0f};
    vk::Rect2D scissor{{0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}};
    vk::PipelineViewportStateCreateInfo viewportState{
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eFront,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = vk::True,
      .depthBiasConstantFactor = 1.25f,
      .depthBiasSlopeFactor = 1.75f,
      .lineWidth = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1 };

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eLess
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{ .attachmentCount = 0 };

    vk::PushConstantRange pushConstantRange{
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .offset = 0,
      .size = sizeof(glm::mat4) * 2   // model + lightSpaceMatrix
    };
    vk::PipelineLayoutCreateInfo layoutInfo{
      .setLayoutCount = 0,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pushConstantRange
    };
    shadowPipelineLayout = vk::raii::PipelineLayout(device, layoutInfo);

    vk::Format depthFormat = findDepthFormat();
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> chain = {
      {
        .stageCount = 1,
        .pStages = &vertStage,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &shadowDynamicState,
        .layout = shadowPipelineLayout,
        .renderPass = nullptr
      },
      {
        .colorAttachmentCount = 0,
        .depthAttachmentFormat = depthFormat
      }
    };

    shadowPipeline = vk::raii::Pipeline(device, nullptr, chain.get<vk::GraphicsPipelineCreateInfo>());

  }

  glm::mat4 createLightSpaceMatrix(){
    glm::vec3 lightDir = glm::normalize(lights[0].position);
    glm::vec3 focus = gameObjects[0]->position;
    glm::mat4 lightView = glm::lookAt(focus +lightDir * 50.0f, focus, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProj = glm::ortho(-40.0f, 40.0f, -40.0f, 40.0f, 0.1f, 150.0f);
    lightProj[1][1] *= -1;
    glm::mat4 lsm = lightProj * lightView;
    return lsm;
  }

  void setupGameObjects() {
    gameObjects[0] = std::make_unique<Plane>();
    gameObjects[1] = std::make_unique<GameObject>();
    gameObjects[2] = std::make_unique<GameObject>();
    gameObjects[3] = std::make_unique<GameObject>();
    gameObjects[4] = std::make_unique<GameObject>();
    gameObjects[5] = std::make_unique<GameObject>();
    gameObjects[6] = std::make_unique<GameObject>();
    gameObjects[7] = std::make_unique<GameObject>();
    gameObjects[8] = std::make_unique<GameObject>();
    gameObjects[9] = std::make_unique<GameObject>();


    gameObjects[0]->name = "plane";
    loadModel(*gameObjects[0], "src/assets/plane/untitled.obj");
    createVertexBuffer(*gameObjects[0]);
    createIndexBuffer(*gameObjects[0]);
    gameObjects[0]->position = {0.0f, 1.0f, 0.0f};
    gameObjects[0]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[0]->scale = {0.15f, 0.15f, 0.15f};


    gameObjects[1]->name = "ground";
    loadModel(*gameObjects[1], "src/assets/ground/ground.obj");
    createVertexBuffer(*gameObjects[1]);
    createIndexBuffer(*gameObjects[1]);
    gameObjects[1]->position = {0.0f, 0.5f, 0.0f};
    gameObjects[1]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[1]->scale = {1.0f, 0.25f, 1.0f};
    gameObjects[1]->lighting = DIFFUSE;


    gameObjects[2]->name = "skybox";
    gameObjects[2]->cullMode = vk::CullModeFlagBits::eFront;
    loadModel(*gameObjects[2], "src/assets/skybox/skybox.obj");
    createVertexBuffer(*gameObjects[2]);
    createIndexBuffer(*gameObjects[2]);
    gameObjects[2]->position = camera.position;
    gameObjects[2]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[2]->scale = {10.0f, 10.0f, 10.0f};
    gameObjects[2]->lighting = NONE;


    gameObjects[3]->name = "buildings1";
    loadModel(*gameObjects[3], "src/assets/buildings/skyscraper1.obj");
    createVertexBuffer(*gameObjects[3]);
    createIndexBuffer(*gameObjects[3]);
    gameObjects[3]->position = {-12.0f, 0.0f, -22.0f};
    gameObjects[3]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[3]->scale = {1.0f, 1.0f, 1.0f};


    gameObjects[4]->name = "buildings2";
    loadModel(*gameObjects[4], "src/assets/buildings/skyscraper1.obj");
    createVertexBuffer(*gameObjects[4]);
    createIndexBuffer(*gameObjects[4]);
    gameObjects[4]->position = {10.0f, 0.0f, 13.0f};
    gameObjects[4]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[4]->scale = {1.0f, 1.0f, 1.0f};


    gameObjects[5]->name = "buildings3";
    loadModel(*gameObjects[5], "src/assets/buildings/skyscraper1.obj");
    createVertexBuffer(*gameObjects[5]);
    createIndexBuffer(*gameObjects[5]);
    gameObjects[5]->position = {15.0f, 0.0f, -7.0f};
    gameObjects[5]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[5]->scale = {1.0f, 1.0f, 1.0f};

    std::cout << "loading missiles" <<std::endl;
    gameObjects[6]->name = "missile1";
    loadModel(*gameObjects[6], "src/assets/missile/missile.obj");
    createVertexBuffer(*gameObjects[6]);
    createIndexBuffer(*gameObjects[6]);
    gameObjects[6]->position = {0.0f, -100.0f, 0.0f};
    gameObjects[6]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[6]->scale = {1.0f, 1.0f, 1.0f};



    gameObjects[7]->name = "missile2";
    loadModel(*gameObjects[7], "src/assets/missile/missile.obj");
    createVertexBuffer(*gameObjects[7]);
    createIndexBuffer(*gameObjects[7]);
    gameObjects[7]->position = {0.0f, -100.0f, 0.0f};
    gameObjects[7]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[7]->scale = {1.0f, 1.0f, 1.0f};



    gameObjects[8]->name = "missile3";
    loadModel(*gameObjects[8], "src/assets/missile/missile.obj");
    createVertexBuffer(*gameObjects[8]);
    createIndexBuffer(*gameObjects[8]);
    gameObjects[8]->position = {0.0f, -100.0f, 0.0f};
    gameObjects[8]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[8]->scale = {1.0f, 1.0f, 1.0f};



    gameObjects[9]->name = "missile4";
    loadModel(*gameObjects[9], "src/assets/missile/missile.obj");
    createVertexBuffer(*gameObjects[9]);
    createIndexBuffer(*gameObjects[9]);
    gameObjects[9]->position = {0.0f, -100.0f, 0.0f};
    gameObjects[9]->rotation = {0.0f, 0.0f, 0.0f};
    gameObjects[9]->scale = {1.0f, 1.0f, 1.0f};
  }

  void setupLights(){
    lights[0].position = glm::vec3{100.0f, 100.0f, 100.0f};
    lights[0].color = glm::vec3{1.0f, 1.0f, 0.75f};

    //lights[1].color = glm::vec3{1.0f, 0.0f, 0.0f};

    //lights[2].color = glm::vec3{0.0f, 1.0f, 0.0f};

  }
};

//---------------- main method --------------
int main() {
  std::cout << "running app" << std::endl;

  if (enableValidationLayers) {
    std::cout << "debug" << std::endl;
  }

  try {
    Application app;
    app.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
