#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

// Shader sources
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;

    void main()
    {
        gl_Position = vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)";

const char* fragmentShaderSourceOriginal = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;

    uniform sampler2D ourTexture;

    void main()
    {
        FragColor = texture(ourTexture, TexCoord);
    }
)";

const char* fragmentShaderSourceGrayscale = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;

    uniform sampler2D ourTexture;

    void main()
    {
        vec4 color = texture(ourTexture, TexCoord);
        float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        FragColor = vec4(vec3(gray), color.a);
    }
)";

const char* fragmentShaderSourceColorFilter = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;

    uniform sampler2D ourTexture;
    uniform float hueShift; // A value between 0.0 and 1.0, representing a hue shift

    // Function to convert RGB to HSL
    vec3 rgbToHsv(vec3 c)
    {
        vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
        vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
        vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

        float d = q.x - min(q.w, q.y);
        float e = 1.0e-10;
        return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
    }

    // Function to convert HSL to RGB
    vec3 hsvToRgb(vec3 c)
    {
        vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    }

    void main()
    {
        vec4 color = texture(ourTexture, TexCoord);

        // Convert to HSV
        vec3 hsv = rgbToHsv(color.rgb);

        // Apply hue shift
        hsv.x = mod(hsv.x + hueShift, 1.0);

        // Convert back to RGB
        FragColor = vec4(hsvToRgb(hsv), color.a);
    }
)";

// Function to compile shaders
unsigned int compileShader(GLenum type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        char* message = (char*)alloca(length * sizeof(char));
        glGetShaderInfoLog(id, length, &length, message);
        std::cout << "Failed to compile " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader!" << std::endl;
        std::cout << message << std::endl;
        glDeleteShader(id);
        return 0;
    }
    return id;
}

// Function to create shader program
unsigned int createShader(const char* vertexShader, const char* fragmentShader) {
    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShader);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// Global image path
std::string imagePath = "image.png"; // Placeholder: user needs to provide an actual image.png in the src folder

int main() {
    // GLFW initialization
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Window creation
    GLFWwindow* window = glfwCreateWindow(1200, 600, "Image Processing", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // GLEW initialization
    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // Load image
    int width, height, nrChannels;
    unsigned char *data = stbi_load(imagePath.c_str(), &width, &height, &nrChannels, 0);
    if (!data) {
        std::cout << "Failed to load image: " << stbi_failure_reason() << std::endl;
        glfwTerminate();
        return -1;
    }

    // Create texture for original image
    unsigned int originalTexture;
    glGenTextures(1, &originalTexture);
    glBindTexture(GL_TEXTURE_2D, originalTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data); // Free image data after creating texture

    // Create FBO for off-screen rendering
    unsigned int fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create texture for grayscale output
    unsigned int grayscaleTexture;
    glGenTextures(1, &grayscaleTexture);
    glBindTexture(GL_TEXTURE_2D, grayscaleTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, grayscaleTexture, 0);

    // Create texture for color filtered output
    unsigned int colorFilterTexture;
    glGenTextures(1, &colorFilterTexture);
    glBindTexture(GL_TEXTURE_2D, colorFilterTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, colorFilterTexture, 0);

    // Check FBO completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    // Quad vertices (normalized device coordinates)
    float vertices[] = {
        // positions        // texture coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, // top-left
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, // bottom-left
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f, // top-right
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f  // bottom-right
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Create shader programs
    unsigned int originalShaderProgram = createShader(vertexShaderSource, fragmentShaderSourceOriginal);
    unsigned int grayscaleShaderProgram = createShader(vertexShaderSource, fragmentShaderSourceGrayscale);
    unsigned int colorFilterShaderProgram = createShader(vertexShaderSource, fragmentShaderSourceColorFilter);


    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        // Clear screen
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- Render original image to left side of screen ---
        glUseProgram(originalShaderProgram);
        glBindTexture(GL_TEXTURE_2D, originalTexture);
        glBindVertexArray(VAO);
        glViewport(0, 0, 600, 600); // Left half of the window
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // --- Render grayscale image to FBO, then to screen ---
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glDrawBuffer(GL_COLOR_ATTACHMENT0); // Render to grayscaleTexture
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Clear background
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(grayscaleShaderProgram);
        glBindTexture(GL_TEXTURE_2D, originalTexture); // Use original image as input
        glBindVertexArray(VAO);
        glViewport(0, 0, width, height); // Render at image resolution
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Back to default framebuffer
        glUseProgram(originalShaderProgram); // Use original shader to draw grayscale texture
        glBindTexture(GL_TEXTURE_2D, grayscaleTexture);
        glViewport(600, 0, 300, 600); // Middle section, displaying grayscale
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


        // --- Render color filtered image to FBO, then to screen ---
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glDrawBuffer(GL_COLOR_ATTACHMENT1); // Render to colorFilterTexture
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Clear background
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(colorFilterShaderProgram);
        glBindTexture(GL_TEXTURE_2D, originalTexture); // Use original image as input
        // Set hue shift uniform (e.g., 0.3 for a noticeable shift)
        float hueShift = 0.5f * (sin(glfwGetTime()) + 1.0f); // Animate hue shift over time
        glUniform1f(glGetUniformLocation(colorFilterShaderProgram, "hueShift"), hueShift);
        glBindVertexArray(VAO);
        glViewport(0, 0, width, height); // Render at image resolution
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Back to default framebuffer
        glUseProgram(originalShaderProgram); // Use original shader to draw color filtered texture
        glBindTexture(GL_TEXTURE_2D, colorFilterTexture);
        glViewport(900, 0, 300, 600); // Right section, displaying color filtered image
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(originalShaderProgram);
    glDeleteProgram(grayscaleShaderProgram);
    glDeleteProgram(colorFilterShaderProgram);
    glDeleteTextures(1, &originalTexture);
    glDeleteTextures(1, &grayscaleTexture);
    glDeleteTextures(1, &colorFilterTexture);
    glDeleteFramebuffers(1, &fbo);

    glfwTerminate();
    return 0;
}