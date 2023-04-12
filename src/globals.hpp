#pragma once

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define ASSERT(x) assert(x)
#define GLFW_CALL(x) ASSERT(x == GLFW_TRUE)
#define VK_CALL(x) ASSERT(x == VK_SUCCESS)

#include <iostream>
#include <vector>

template <typename T>
std::ostream &operator<<(std::ostream &stream, const std::vector<T> &vec) {
  stream << "[ ";
  for (const auto &item : vec)
    stream << item << ", ";
  stream << "\b\b ]";
  return stream;
}
