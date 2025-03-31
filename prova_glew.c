// Amb l'última actualització ha deixat d'anar l'openMSX i diu error de glew. Analitzem-ho amb aquest programa
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  // Initialize GLFW
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }
  
  // Set OpenGL version hints before creating window
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE); // Use compatibility profile
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Make window invisible
  GLFWwindow* window = glfwCreateWindow(640, 480, "OpenGL Test", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    return -1;
  }
  
  // Make the window's context current
  glfwMakeContextCurrent(window);
  
  // Print OpenGL version before GLEW init
  printf("Pre-GLEW OpenGL version: %s\n", glGetString(GL_VERSION));
  
  // Initialize GLEW with experimental flag
  glewExperimental = GL_TRUE; // Needed for core profile
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(err));
    // Continue anyway to print diagnostic information
    printf("Continuing despite GLEW initialization failure...\n");
  } else {
    printf("GLEW initialized successfully!\n");
  }
  
  // Clear any error that might have been set by glewInit
  // This is important as glewInit() often generates an OpenGL error that should be cleared
  while (glGetError() != GL_NO_ERROR) {
    // Clear all errors
  }
  
  // Print GLEW version
  printf("GLEW version: %s\n", glewGetString(GLEW_VERSION));
  
  // Check if GLEW2 is available
  if (glewIsSupported("GL_VERSION_2_0")) {
    printf("GLEW 2.0 is supported\n");
  } else {
    printf("GLEW 2.0 is not supported\n");
  }
  
  // Basic OpenGL check
  const GLubyte* glVersion = glGetString(GL_VERSION);
  if (glVersion) {
    printf("OpenGL version: %s\n", glVersion);
  } else {
    fprintf(stderr, "Failed to get OpenGL version.\n");
  }
  
  // Get OpenGL vendor and renderer
  const GLubyte* vendor = glGetString(GL_VENDOR);
  const GLubyte* renderer = glGetString(GL_RENDERER);
  
  if (vendor) {
    printf("OpenGL vendor: %s\n", vendor);
  }
  if (renderer) {
    printf("OpenGL renderer: %s\n", renderer);
  }
  
  // Clean up
  glfwDestroyWindow(window);
  glfwTerminate();
  
  return 0;
}
