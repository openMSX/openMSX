// Simple OpenGL test program
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
  glutCreateWindow("Simple OpenGL Test");
  
  // Register display callback
  glutDisplayFunc(display);
  
  // Print OpenGL information
  const GLubyte* version = glGetString(GL_VERSION);
  if (version) {
    printf("OpenGL version: %s\n", version);
  } else {
    printf("Failed to get OpenGL version\n");
  }
  
  const GLubyte* vendor = glGetString(GL_VENDOR);
  if (vendor) {
    printf("OpenGL vendor: %s\n", vendor);
  }
  
  const GLubyte* renderer = glGetString(GL_RENDERER);
  if (renderer) {
    printf("OpenGL renderer: %s\n", renderer);
  }
  
  const GLubyte* extensions = glGetString(GL_EXTENSIONS);
  if (extensions) {
    printf("First few OpenGL extensions: %.100s...\n", extensions);
  }
  
  // Test some basic OpenGL functionality
  printf("Testing basic OpenGL functionality...\n");
  
  // Test glClear
  glClear(GL_COLOR_BUFFER_BIT);
  GLenum err = glGetError();
  if (err == GL_NO_ERROR) {
    printf("glClear works correctly\n");
  } else {
    printf("glClear error: 0x%x\n", err);
  }
  
  // Test glViewport
  glViewport(0, 0, 640, 480);
  err = glGetError();
  if (err == GL_NO_ERROR) {
    printf("glViewport works correctly\n");
  } else {
    printf("glViewport error: 0x%x\n", err);
  }
  
  printf("Basic OpenGL test completed successfully\n");
  
  return 0;
}
