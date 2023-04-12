#include <array>
#include <memory>

#include "globals.hpp"

#include "application.hpp"

auto chungus_application::gflw_window_deleter::operator()(GLFWwindow *window) {
  glfwDestroyWindow(window);
}

auto chungus_application::create_glfw_window(const uint32_t width,
                                             const uint32_t height,
                                             const std::string_view title) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  auto *new_window =
      glfwCreateWindow(width, height, title.data(), nullptr, nullptr);

  ASSERT(new_window);
  return new_window;
}

auto chungus_application::create_vulkan_instance(const std::string_view title) {

  VkInstance instance = {};

  {
    std::vector<const char *> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    };

    uint32_t glfw_extension_count = 0;
    auto **glfw_extensions_ptr =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    extensions.insert(extensions.end(), glfw_extensions_ptr,
                      glfw_extensions_ptr + glfw_extension_count);

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = title.data();
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "RAW";
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = validation_layers.size();
    create_info.ppEnabledLayerNames = validation_layers.data();
    create_info.enabledExtensionCount = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();

    VK_CALL(vkCreateInstance(&create_info, nullptr, &instance));
  }

  ASSERT(instance != nullptr);
  return instance;
}

chungus_application::chungus_application(const uint32_t height,
                                         const uint32_t width,
                                         const std::string_view title)
    : window_height(height), window_width(width), window_title(title) {
  GLFW_CALL(glfwInit());

  window.reset(create_glfw_window(window_width, window_height, window_title));
  instance = create_vulkan_instance(window_title);

  initialize_graphics();
  main_loop();
}

chungus_application::~chungus_application() { glfwTerminate(); }

void chungus_application::initialize_graphics() {
  // get surface
  VkSurfaceKHR surface = {};
  VK_CALL(glfwCreateWindowSurface(instance, window.get(), nullptr, &surface));

  // get physical device
  VkPhysicalDevice physical_device = {};
  {
    uint32_t device_count = 0;
    VK_CALL(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
    ASSERT(device_count > 0);

    std::vector<VkPhysicalDevice> devices{device_count};
    VK_CALL(
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data()));

    for (const auto &device : devices) {
      VkPhysicalDeviceProperties properties = {};
      vkGetPhysicalDeviceProperties(device, &properties);
      std::cout << "physical device: " << properties.deviceName << std::endl;
    }

    physical_device = devices[0];
  }

  // get grpahics queue index
  uint32_t graphics_queue_family_index = -1;
  {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families{queue_family_count};
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families.data());

    for (int index = 0; index < queue_families.size(); index += 1) {
      if (queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        VkBool32 present_support = {};
        VK_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device, index, surface, &present_support));

        if (present_support) {
          graphics_queue_family_index = 0;
          break;
        }
      }
    }

    ASSERT(graphics_queue_family_index != -1);
  }

  // get the right surface format supported by physical device
  VkSurfaceFormatKHR surface_format = {};
  {
    uint32_t format_count = 0;
    VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                                 &format_count, nullptr));

    std::vector<VkSurfaceFormatKHR> surface_formats{format_count};
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                         &format_count, surface_formats.data());

    for (const auto &entry : surface_formats) {
      if ((entry.format == VK_FORMAT_B8G8R8A8_SRGB) &&
          (entry.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)) {
        surface_format = entry;
        break;
      }
    }
  }

  VkSurfaceCapabilitiesKHR device_capabilities = {};
  VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                                    &device_capabilities));

  // get logical vulkan device
  VkDevice device = {};
  {
    const auto queue_priority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos = {{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphics_queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    }};

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pEnabledFeatures = nullptr;
    device_create_info.queueCreateInfoCount = queue_create_infos.size();
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.enabledLayerCount = validation_layers.size();
    device_create_info.ppEnabledLayerNames = validation_layers.data();
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();

    VK_CALL(
        vkCreateDevice(physical_device, &device_create_info, nullptr, &device));
  }

  // create main graphics queue
  VkQueue queue = {};
  vkGetDeviceQueue(device, graphics_queue_family_index, 0, &queue);

  // create swapchain and set extents
  VkSwapchainKHR swapchain = {};
  VkExtent2D swap_extent = {static_cast<uint32_t>(window_width),
                            static_cast<uint32_t>(window_height)};
  {
    // physical device has max limit
    if (device_capabilities.currentExtent.width != UINT32_MAX)
      swap_extent = device_capabilities.currentExtent;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    {
      swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      swapchain_create_info.clipped = VK_TRUE;
      swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
      swapchain_create_info.preTransform = device_capabilities.currentTransform;
      swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
      swapchain_create_info.surface = surface;
      swapchain_create_info.imageArrayLayers = 1;
      swapchain_create_info.imageColorSpace = surface_format.colorSpace;
      swapchain_create_info.imageExtent = swap_extent;
      swapchain_create_info.imageFormat = surface_format.format;
      swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain_create_info.minImageCount =
          device_capabilities.minImageCount + 1;
      swapchain_create_info.imageUsage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VK_CALL(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr,
                                 &swapchain));
  }

  std::vector<VkImage> swap_images;
  {
    uint32_t image_count;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);

    swap_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count,
                            swap_images.data());
  }

  std::vector<VkImageView> swap_image_views;
  swap_image_views.resize(swap_images.size());
  {

    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.format = surface_format.format;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.layerCount = 1;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    for (int idx = 0; idx < swap_images.size(); idx += 1) {
      view_create_info.image = swap_images[idx];
      VK_CALL(vkCreateImageView(device, &view_create_info, NULL,
                                &swap_image_views[idx]) != VK_SUCCESS);
    }
  }
}

void chungus_application::main_loop() {
  while (!glfwWindowShouldClose(window.get())) {
    glfwPollEvents();
  }
}
