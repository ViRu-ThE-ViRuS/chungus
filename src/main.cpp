#include <iostream>

#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>

#include "boost/thread.hpp"

int main() {
  std::cout << glfwInit() << std::endl;
  glm::vec3 v{1, 2, 3};
  boost::thread t;
  return 0;
}
