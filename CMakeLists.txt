cmake_minimum_required(VERSION 3.10)
project(ImageProcessing)

set(CMAKE_CXX_STANDARD 17)

# Find OpenGL and GLFW
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# Include directories
include_directories(include)

# Add executable
add_executable(app src/main.cpp)

# Link libraries
target_link_libraries(app PRIVATE OpenGL::GL glfw GLEW::GLEW)

add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/src/image.png
    $<TARGET_FILE_DIR:app>/image.png
)

