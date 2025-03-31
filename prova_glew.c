// Amb l'última actualització ha deixat d'anar l'openMSX i diu error de glew. Analitzem-ho amb aquest programa
#include <GL/glew2.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  // Print GLEW version
  printf("GLEW version: %s\n", glewGetString(GLEW_VERSION));
  
  // Check if GLEW2 is available
  if (glewIsSupported("GLEW_VERSION_2_0")) {
    printf("GLEW 2.0 is supported\n");
  } else {
    printf("GLEW 2.0 is not supported\n");
  }
  
  // Try to initialize GLEW
  // Note: This will likely still fail without a proper OpenGL context,
  // but we're checking what version of GLEW is installed
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(err));
    fprintf(stderr, "This is expected without a proper OpenGL context\n");
  } else {
    printf("GLEW initialized successfully!\n");
    
    // Basic OpenGL check
    const GLubyte* glVersion = glGetString(GL_VERSION);
    if (glVersion) {
      printf("OpenGL version: %s\n", glVersion);
    } else {
      fprintf(stderr, "Failed to get OpenGL version.\n");
    }
  }
  
  return 0;
}
