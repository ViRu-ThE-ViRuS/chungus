#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "globals.hpp"

#include <vulkan/vulkan.h>

class chungus_application {
private:
  static constexpr std::array<const char *, 1> validation_layers{
      "VK_LAYER_KHRONOS_validation"};

  static constexpr std::array<const char *, 2> device_extensions{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};

  struct gflw_window_deleter {
    auto operator()(GLFWwindow *);
  };

  static auto create_glfw_window(const uint32_t, const uint32_t,
                                 const std::string_view);
  static auto create_vulkan_instance(const std::string_view);

  static auto create_default_shaders();

public:
  explicit chungus_application(const uint32_t, const uint32_t,
                               const std::string_view);
  ~chungus_application();

private:
  void initialize_graphics();
  void main_loop();
  void cleanup_graphics();

  const size_t window_height, window_width;
  const std::string_view window_title;

  struct render_info_t {
    VkDevice device = {};                          // logical device
    VkQueue queue = {};                            // graphics queue
    VkSwapchainKHR swapchain = {};                 // primary swapchain
    std::vector<VkCommandBuffer> cmd_buffers = {}; // draw calls
    std::vector<VkImage> swap_images = {};         // images
  } render_info;

  // render_info_t render_info;

  std::unique_ptr<GLFWwindow, gflw_window_deleter> window;
  VkInstance instance;
};
