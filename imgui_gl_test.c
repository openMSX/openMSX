// Test program using imgui's OpenGL loader
#include <stdio.h>
#include <stdlib.h>

// Include the same OpenGL loader that imgui uses
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "src/3rdparty/imgui/imgui_impl_opengl3_loader.h"

// We need to include the implementation
#define IMGL3W_IMPL
#include "src/3rdparty/imgui/imgui_impl_opengl3_loader.h"

#include <GLFW/glfw3.h>

int main() {
    printf("Testing imgui OpenGL loader...\n");
    
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    
    // Set OpenGL version hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Make window invisible
    
    // Create a window
    GLFWwindow* window = glfwCreateWindow(640, 480, "ImGui GL Test", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    
    // Make the window's context current
    glfwMakeContextCurrent(window);
    
    // Initialize imgui's GL loader
    int result = imgl3wInit();
    if (result != 0) {
        fprintf(stderr, "Failed to initialize imgui GL loader: %d\n", result);
    } else {
        printf("imgui GL loader initialized successfully\n");
    }
    
    // Try to get OpenGL version
    const GLubyte* version = glGetString(GL_VERSION);
    if (version) {
        printf("OpenGL version: %s\n", version);
    } else {
        printf("Failed to get OpenGL version\n");
    }
    
    // Clean up
    glfwDestroyWindow(window);
    glfwTerminate();
    
    printf("imgui OpenGL loader test completed\n");
    
    return 0;
}
