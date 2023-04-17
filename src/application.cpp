#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

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

auto chungus_application::create_default_shaders() {
  std::ifstream vertex_input{"shaders/default.vert.spv",
                             std::ios::ate | std::ios::binary};
  ASSERT(vertex_input.is_open());
  uint32_t vertex_file_size = vertex_input.tellg();
  ASSERT(vertex_file_size % sizeof(uint32_t) == 0);

  std::ifstream fragment_input{"shaders/default.frag.spv",
                               std::ios::ate | std::ios::binary};
  ASSERT(fragment_input.is_open());
  uint32_t fragment_file_size = fragment_input.tellg();
  ASSERT(fragment_file_size % sizeof(uint32_t) == 0);

  std::vector<char> vertex_shader(vertex_file_size / sizeof(char));
  {
    vertex_input.seekg(0);
    vertex_input.read(vertex_shader.data(), vertex_file_size);
    vertex_input.close();
  }

  std::vector<char> fragment_shader(fragment_file_size / sizeof(char));
  {
    fragment_input.seekg(0);
    fragment_input.read(fragment_shader.data(), fragment_file_size);
    fragment_input.close();
  }

  return std::make_tuple(std::move(vertex_shader), std::move(fragment_shader));
}

chungus_application::chungus_application(const uint32_t height,
                                         const uint32_t width,
                                         const std::string_view title)
    : window_height(height), window_width(width), window_title(title) {
  GLFW_CALL(glfwInit());

  window.reset(create_glfw_window(window_width, window_height, window_title));
  instance = create_vulkan_instance(window_title);

  render_info = {};

  initialize_graphics();
  main_loop();
  cleanup_graphics();
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

    VK_CALL(vkCreateDevice(physical_device, &device_create_info, nullptr,
                           &render_info.device));
  }

  // create main graphics queue
  vkGetDeviceQueue(render_info.device, graphics_queue_family_index, 0,
                   &render_info.queue);

  // create swapchain and set extents
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

    VK_CALL(vkCreateSwapchainKHR(render_info.device, &swapchain_create_info,
                                 nullptr, &render_info.swapchain));
  }

  // create swapchain images and views
  {
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(render_info.device, render_info.swapchain,
                            &image_count, nullptr);
    ASSERT(image_count != 0);

    render_info.swap_images.resize(image_count);
    vkGetSwapchainImagesKHR(render_info.device, render_info.swapchain,
                            &image_count, render_info.swap_images.data());
  }

  std::vector<VkImageView> swap_image_views;
  swap_image_views.resize(render_info.swap_images.size());
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

    for (int idx = 0; idx < render_info.swap_images.size(); idx += 1) {
      view_create_info.image = render_info.swap_images[idx];
      VK_CALL(vkCreateImageView(render_info.device, &view_create_info, nullptr,
                                &swap_image_views[idx]) != VK_SUCCESS);
    }
  }

  // create vertex buffer and copy in a demo triangle
  std::vector<float> vertices = {0.0, -0.5, 0.5, 0.5, -0.5, 0.5};
  VkBuffer vertex_buffer = {};
  {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.size = sizeof(vertices[0]) * vertices.size();
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CALL(vkCreateBuffer(render_info.device, &buffer_info, nullptr,
                           &vertex_buffer));
  }

  VkDeviceMemory vertex_buffer_memory = {};
  {
    VkMemoryRequirements mem_requirements = {};
    vkGetBufferMemoryRequirements(render_info.device, vertex_buffer,
                                  &mem_requirements);

    uint32_t memory_type_index = -1;
    {
      VkPhysicalDeviceMemoryProperties mem_properties = {};
      vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

      for (int i = 0; i < mem_properties.memoryTypeCount; i += 1) {
        if ((mem_requirements.memoryTypeBits & (1 << i)) and
            ((mem_properties.memoryTypes[i].propertyFlags &
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
          memory_type_index = i;
          break;
        }
      }

      ASSERT(memory_type_index != -1);
    }

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.memoryTypeIndex = memory_type_index;
    alloc_info.allocationSize = mem_requirements.size;

    VK_CALL(vkAllocateMemory(render_info.device, &alloc_info, nullptr,
                             &vertex_buffer_memory));
    VK_CALL(vkBindBufferMemory(render_info.device, vertex_buffer,
                               vertex_buffer_memory, 0));
  }

  // copy data into buffer
  {
    void *data = nullptr;
    VK_CALL(vkMapMemory(render_info.device, vertex_buffer_memory, 0,
                        VK_WHOLE_SIZE, 0, &data));
    std::memcpy(data, vertices.data(), sizeof(float) * vertices.size());

    VkMappedMemoryRange memory_range = {};
    {
      memory_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      memory_range.memory = vertex_buffer_memory;
      memory_range.size = VK_WHOLE_SIZE;
      memory_range.offset = 0;
    }

    VK_CALL(vkFlushMappedMemoryRanges(render_info.device, 1, &memory_range));
    vkUnmapMemory(render_info.device, vertex_buffer_memory);
  }

  // create render pass
  VkRenderPass render_pass = {};
  {
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = surface_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VK_CALL(vkCreateRenderPass(render_info.device, &render_pass_info, nullptr,
                               &render_pass));
  }

  // vertex and fragment shaders
  VkShaderModule vertex_shader_module = {};
  VkShaderModule fragment_shader_module = {};
  {
    auto [vertex_shader, fragment_shader] = create_default_shaders();

    VkShaderModuleCreateInfo vs_shader_info = {};
    vs_shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vs_shader_info.codeSize = vertex_shader.size();
    vs_shader_info.pCode = reinterpret_cast<uint32_t *>(vertex_shader.data());

    VkShaderModuleCreateInfo f_shader_info = {};
    f_shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    f_shader_info.codeSize = fragment_shader.size();
    f_shader_info.pCode = reinterpret_cast<uint32_t *>(fragment_shader.data());

    VK_CALL(vkCreateShaderModule(render_info.device, &vs_shader_info, nullptr,
                                 &vertex_shader_module));
    VK_CALL(vkCreateShaderModule(render_info.device, &f_shader_info, nullptr,
                                 &fragment_shader_module));
  }

  // create graphics pipeline layout
  VkPipelineLayout pipeline_layout = {};
  {
    VkPipelineLayoutCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VK_CALL(vkCreatePipelineLayout(render_info.device, &create_info, nullptr,
                                   &pipeline_layout));
  }

  // create render pipeline
  VkPipeline pipeline = {};
  {
    // shader stages
    VkPipelineShaderStageCreateInfo vertex_stage = {};
    vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage.module = vertex_shader_module;
    vertex_stage.pName = "main";

    VkPipelineShaderStageCreateInfo fragment_stage = {};
    fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_stage.module = fragment_shader_module;
    fragment_stage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages[] = {vertex_stage,
                                                               fragment_stage};

    // bindings
    VkVertexInputBindingDescription binding_info = {};
    binding_info.stride = sizeof(float) * 2;
    binding_info.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription position_attr = {};
    position_attr.binding = 0;
    position_attr.location = 0;
    position_attr.offset = 0;
    position_attr.format = VK_FORMAT_R32G32_SFLOAT;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = NULL;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &binding_info;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &position_attr;

    // pipeline
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = swap_extent.width;
    viewport.height = swap_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swap_extent;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0;
    rasterizer.depthBiasClamp = 0.0;
    rasterizer.depthBiasSlopeFactor = 0.0;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.minSampleShading = 0.0;
    multisampling.pSampleMask = NULL;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend_att = {};
    blend_att.blendEnable = VK_FALSE;
    blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.colorBlendOp = VK_BLEND_OP_ADD;
    blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
    blend_att.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blending = {};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.logicOpEnable = VK_FALSE;
    blending.logicOp = VK_LOGIC_OP_COPY;
    blending.attachmentCount = 1;
    blending.pAttachments = &blend_att;
    blending.blendConstants[0] = 0.0f;
    blending.blendConstants[1] = 0.0f;
    blending.blendConstants[2] = 0.0f;
    blending.blendConstants[3] = 0.0f;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages->data();
    pipeline_info.pVertexInputState = &vertexInputInfo;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &blending;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    VK_CALL(vkCreateGraphicsPipelines(render_info.device, VK_NULL_HANDLE, 1,
                                      &pipeline_info, nullptr, &pipeline));
  }

  std::vector<VkFramebuffer> frame_buffers(swap_image_views.size());
  {
    VkFramebufferCreateInfo create_info = {};
    {
      create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      create_info.renderPass = render_pass;
      create_info.attachmentCount = 1;
      create_info.width = swap_extent.width;
      create_info.height = swap_extent.height;
      create_info.layers = 1;
    }

    for (int index = 0; index < frame_buffers.size(); index += 1) {
      create_info.pAttachments = &swap_image_views[index];
      VK_CALL(vkCreateFramebuffer(render_info.device, &create_info, nullptr,
                                  &frame_buffers[index]));
    }
  }

  VkCommandPool cmd_pool = {};
  {
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = graphics_queue_family_index;
    VK_CALL(vkCreateCommandPool(render_info.device, &create_info, nullptr,
                                &cmd_pool));
  }

  render_info.cmd_buffers.resize(swap_image_views.size());
  {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = cmd_pool;
    alloc_info.commandBufferCount = render_info.cmd_buffers.size();

    // create command buffers
    VK_CALL(vkAllocateCommandBuffers(render_info.device, &alloc_info,
                                     render_info.cmd_buffers.data()));
  }

  // record render comands
  {
    // begin all cmd buffers
    {
      VkCommandBufferBeginInfo begin_info = {};
      begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

      for (int index = 0; index < render_info.cmd_buffers.size(); index += 1)
        VK_CALL(
            vkBeginCommandBuffer(render_info.cmd_buffers[index], &begin_info));
    }

    VkClearValue clear = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo pass_info = {};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_info.renderPass = render_pass;
    // pass_info.framebuffer = frame_buffers[index];
    pass_info.renderArea.offset = {0, 0};
    pass_info.renderArea.extent = {swap_extent.width, swap_extent.height};
    pass_info.clearValueCount = 1;
    pass_info.pClearValues = &clear;

    VkDeviceSize offsets[] = {0};
    const uint32_t vertex_count = 3, instance_count = 1;

    // issue draw calls
    // clang-format off
    for(int index = 0; index < render_info.cmd_buffers.size(); index += 1) {
      pass_info.framebuffer = frame_buffers[index];

      vkCmdBeginRenderPass(render_info.cmd_buffers[index], &pass_info, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(render_info.cmd_buffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      vkCmdBindVertexBuffers(render_info.cmd_buffers[index], 0, 1, &vertex_buffer, offsets);
      vkCmdDraw(render_info.cmd_buffers[index], vertex_count, instance_count, 0, index);
      vkCmdEndRenderPass(render_info.cmd_buffers[index]);
    }
    // clang-format on

    // end all cmd buffers
    {
      for (int index = 0; index < render_info.cmd_buffers.size(); index += 1)
        VK_CALL(vkEndCommandBuffer(render_info.cmd_buffers[index]));
    }
  }
}

void chungus_application::main_loop() {
  // create sync primitives
  // clang-format off
  const uint32_t images_in_flight = 2;
  std::vector<VkSemaphore> sem_image_available(images_in_flight);
  std::vector<VkSemaphore> sem_render_finished(images_in_flight);
  std::vector<VkFence> fen_active(images_in_flight);
  std::vector<VkFence> fen_images(render_info.swap_images.size(), VK_NULL_HANDLE);;
  {
    VkSemaphoreCreateInfo sem_info = {};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fen_info = {};
    fen_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fen_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(int index = 0; index < images_in_flight; index += 1) {
      VK_CALL(vkCreateSemaphore(render_info.device, &sem_info, nullptr, &sem_image_available[index]));
      VK_CALL(vkCreateSemaphore(render_info.device, &sem_info, nullptr, &sem_render_finished[index]));
      VK_CALL(vkCreateFence(render_info.device, &fen_info, nullptr, &fen_active[index]));
    }
  }
  // clang-format on

  // loop
  uint32_t active_sync_index = 0;
  while (!glfwWindowShouldClose(window.get())) {
    glfwPollEvents();

    VK_CALL(vkWaitForFences(render_info.device, 1,
                            &fen_active[active_sync_index], VK_TRUE,
                            UINT64_MAX));

    uint32_t image_index = 0;
    VK_CALL(vkAcquireNextImageKHR(
        render_info.device, render_info.swapchain, UINT64_MAX,
        sem_image_available[active_sync_index], VK_NULL_HANDLE, &image_index));

    if (fen_images[image_index] != VK_NULL_HANDLE)
      VK_CALL(vkWaitForFences(render_info.device, 1, &fen_images[image_index],
                              VK_TRUE, UINT64_MAX));

    fen_images[image_index] = fen_active[active_sync_index];
    VkSemaphore sem_wait[] = {sem_image_available[active_sync_index]};
    VkSemaphore sem_signal[] = {sem_render_finished[active_sync_index]};
    VkPipelineStageFlags stages_wait[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = sem_wait;
    submit_info.pWaitDstStageMask = stages_wait;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &render_info.cmd_buffers[image_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = sem_signal;
    VK_CALL(
        vkResetFences(render_info.device, 1, &fen_active[active_sync_index]));

    // drawcall
    VK_CALL(vkQueueSubmit(render_info.queue, 1, &submit_info,
                          fen_active[active_sync_index]));

    VkPresentInfoKHR present_info;
    {
      present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      present_info.pNext = 0;
      present_info.waitSemaphoreCount = 1;
      present_info.pWaitSemaphores = sem_signal;
      present_info.swapchainCount = 1;
      present_info.pSwapchains = &render_info.swapchain;
      present_info.pImageIndices = &image_index;
      present_info.pResults = NULL;
    }

    // present
    vkQueuePresentKHR(render_info.queue, &present_info);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    active_sync_index = (active_sync_index + 1) % images_in_flight;
  }
}

void chungus_application::cleanup_graphics() {
  // TODO(vir): read image to cpu memory
}
