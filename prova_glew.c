// Amb l'última actualització ha deixat d'anar l'openMSX i diu error de glew. Analitzem-ho amb aquest programa
#include <GL/glew.h>
#include <stdio.h>

int main() {
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "GLEW initialization failed!\n");
    fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(err));
    return 1;
  }

  printf("GLEW initialized successfully!\n");

  // Basic OpenGL check (e.g., get OpenGL version)
  const GLubyte* glVersion = glGetString(GL_VERSION);
  if (glVersion) {
    printf("OpenGL version: %s\n", glVersion);
  } else {
    fprintf(stderr, "Failed to get OpenGL version.\n");
    return 1;
  }

  return 0;
}
