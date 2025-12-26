cmake_minimum_required(VERSION 3.10)

project(RobotArm LANGUAGES C CXX)

# C++ 표준
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# Executable
# =========================
add_executable(RobotArm
    src/main.cpp
    glad.c
    stb_image.cpp
)

# =========================
# Include directories
# =========================
target_include_directories(RobotArm PRIVATE
    src
    .
    src/learnopengl
)

# =========================
# Dependencies
# =========================
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

target_link_libraries(RobotArm
    OpenGL::GL
    glfw
    assimp
    glm::glm
)

# =========================
# Runtime working directory
# =========================
set_target_properties(RobotArm PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# =========================
# Windows warning level
# =========================
if (MSVC)
    target_compile_options(RobotArm PRIVATE /W4)
endif()
