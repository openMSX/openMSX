// GLEW utility functions to handle initialization issues
#ifndef GLEW_UTILS_H
#define GLEW_UTILS_H

#include <GL/glew.h>
#include <stdio.h>
#include <stdbool.h>

// Initialize GLEW and handle the "Missing GL version" error gracefully
// Returns true if GLEW is usable (even if glewInit reports an error)
static inline bool initGlewGracefully(bool verbose) {
    // Initialize GLEW with experimental flag
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    
    if (err != GLEW_OK) {
        if (verbose) {
            fprintf(stderr, "GLEW initialization reported error: %s (code %d)\n", 
                    glewGetErrorString(err), err);
            
            // Check if this is the "Missing GL version" error (code 1)
            if (err == 1) {
                fprintf(stderr, "This is a known issue with some GLEW versions.\n");
                fprintf(stderr, "Checking if GLEW is still usable despite the error...\n");
            }
        }
    } else if (verbose) {
        printf("GLEW initialized successfully!\n");
    }
    
    // Clear any error that might have been set by glewInit
    while (glGetError() != GL_NO_ERROR) {
        // Just clear all errors
    }
    
    // Check if GLEW is actually usable despite the error
    bool isUsable = glewIsSupported("GL_VERSION_2_0");
    
    if (verbose) {
        if (isUsable) {
            printf("GLEW is usable (OpenGL 2.0+ functions available)\n");
        } else {
            fprintf(stderr, "GLEW is not usable (OpenGL 2.0 functions unavailable)\n");
        }
    }
    
    return isUsable;
}

#endif // GLEW_UTILS_H
