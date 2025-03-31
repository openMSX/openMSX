// Test program using imgui's OpenGL loader
#include <stdio.h>
#include <stdlib.h>

// Include the same OpenGL loader that imgui uses
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "src/3rdparty/imgui/imgui_impl_opengl3_loader.h"

int main() {
    // Try to load OpenGL functions without a window
    // This won't work for most functions, but we can check if the loader compiles
    
    printf("Testing imgui OpenGL loader...\n");
    
    // Try to get OpenGL version (will likely fail without context)
    const GLubyte* version = glGetString(GL_VERSION);
    if (version) {
        printf("OpenGL version: %s\n", version);
    } else {
        printf("Failed to get OpenGL version (expected without context)\n");
    }
    
    printf("imgui OpenGL loader test completed\n");
    
    return 0;
}
