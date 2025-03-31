// Amb l'última actualització ha deixat d'anar l'openMSX i diu error de glew. Analitzem-ho amb aquest programa
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>

int main() {
  // Initialize GLFW
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return 1;
  }

  // Create a windowed mode window and its OpenGL context
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Make window invisible
  GLFWwindow* window = glfwCreateWindow(640, 480, "GLEW Test", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    return 1;
  }

  // Make the window's context current
  glfwMakeContextCurrent(window);
  
  // Initialize GLEW
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "GLEW initialization failed!\n");
    fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(err));
    glfwTerminate();
    return 1;
  }

  printf("GLEW initialized successfully!\n");

  // Basic OpenGL check (e.g., get OpenGL version)
  const GLubyte* glVersion = glGetString(GL_VERSION);
  if (glVersion) {
    printf("OpenGL version: %s\n", glVersion);
  } else {
    fprintf(stderr, "Failed to get OpenGL version.\n");
    glfwTerminate();
    return 1;
  }

  // Clean up
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
