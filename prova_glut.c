// Simple program to check OpenGL and GLEW using GLUT
#include <GL/glew.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "glew_utils.h"

void display() {
  glClear(GL_COLOR_BUFFER_BIT);
  glutSwapBuffers();
}

int main(int argc, char** argv) {
  // Initialize GLUT
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(640, 480);
  glutCreateWindow("OpenGL Test with GLUT");
  
  // Register display callback
  glutDisplayFunc(display);
  
  // Print OpenGL version before GLEW init
  printf("Pre-GLEW OpenGL version: %s\n", glGetString(GL_VERSION));
  
  // Force a complete OpenGL context before GLEW init
  glClear(GL_COLOR_BUFFER_BIT);
  glutSwapBuffers();
  
  // Initialize GLEW directly
  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != GLEW_OK) {
    fprintf(stderr, "GLEW initialization error: %s (code %d)\n", 
            glewGetErrorString(glewError), glewError);
    return -1;
  }
  printf("GLEW initialization successful\n");
  
  // Clear any error that might have been set by glewInit
  GLenum glErr = glGetError();
  if (glErr != GL_NO_ERROR) {
    printf("OpenGL error after GLEW init: 0x%x (this is often normal)\n", glErr);
  }
  
  // Also try using our utility function for comparison
  bool glewUsable = initGlewGracefully(true);
  if (!glewUsable) {
    fprintf(stderr, "initGlewGracefully reports GLEW is not usable\n");
  } else {
    printf("initGlewGracefully reports GLEW is usable\n");
  }
  
  // Print OpenGL and GLEW information
  printf("GLEW version: %s\n", glewGetString(GLEW_VERSION));
  
  const GLubyte* glVersion = glGetString(GL_VERSION);
  if (glVersion) {
    printf("OpenGL version: %s\n", glVersion);
  } else {
    fprintf(stderr, "Failed to get OpenGL version.\n");
  }
  
  const GLubyte* vendor = glGetString(GL_VENDOR);
  const GLubyte* renderer = glGetString(GL_RENDERER);
  
  if (vendor) {
    printf("OpenGL vendor: %s\n", vendor);
  }
  if (renderer) {
    printf("OpenGL renderer: %s\n", renderer);
  }
  
  // Start the main loop (this will show the window)
  // Comment this out if you just want to check versions without showing a window
  // glutMainLoop();
  
  return 0;
}
