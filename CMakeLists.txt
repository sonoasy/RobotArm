cmake_minimum_required(VERSION 3.10)
project(OpenGLProject)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(main main.cpp)

# glfw → 그냥 glfw 만 링크
target_link_libraries(main PRIVATE glfw glad::glad glm::glm)
