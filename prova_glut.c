// Simple program to check OpenGL and GLEW using GLUT
#include <GL/glew.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>

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
  
  // Initialize GLEW
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(err));
    // Continue anyway - on some systems GLEW reports an error but still works
  } else {
    printf("GLEW initialized successfully!\n");
  }
  
  // Clear any error that might have been set by glewInit
  // This is important as glewInit() often generates an OpenGL error that should be cleared
  while (glGetError() != GL_NO_ERROR) {
    // Clear all errors
  }
  
  // Clear any error that might have been set by glewInit
  GLenum glErr = glGetError();
  if (glErr != GL_NO_ERROR) {
    printf("OpenGL error after GLEW init: 0x%x (this is often normal)\n", glErr);
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
