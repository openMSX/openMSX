// Alternative GLEW initialization test
#include <GL/glew.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
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
  glutCreateWindow("GLEW Alternative Init Test");
  
  // Register display callback
  glutDisplayFunc(display);
  
  printf("OpenGL version before GLEW: %s\n", glGetString(GL_VERSION));
  
  // Try alternative GLEW initialization approach
  printf("Attempting alternative GLEW initialization...\n");
  
  // Use our utility function to initialize GLEW
  bool glewUsable = initGlewGracefully(true);
  
  // Test if we can use GLEW-provided function pointers despite the error
  printf("\nTesting GLEW function availability:\n");
  
  // Test if we can get extension support info
  printf("GL_ARB_vertex_buffer_object supported: %s\n", 
         glewIsSupported("GL_ARB_vertex_buffer_object") ? "YES" : "NO");
  printf("GL_VERSION_2_0 supported: %s\n", 
         glewIsSupported("GL_VERSION_2_0") ? "YES" : "NO");
  
  // Test if we can use a GLEW-provided function
  if (GLEW_VERSION_2_0) {
    printf("GLEW reports OpenGL 2.0 is available\n");
    
    // Try to use a function that requires OpenGL 2.0
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
      printf("Successfully called glCreateShader (OpenGL 2.0 function)\n");
      glDeleteShader(shader);
    } else {
      printf("Error calling glCreateShader: 0x%x\n", error);
    }
  } else {
    printf("GLEW reports OpenGL 2.0 is NOT available\n");
  }
  
  printf("\nConclusion: GLEW %s usable despite initialization error\n", 
         GLEW_VERSION_2_0 ? "IS" : "is NOT");
  
  return 0;
}
