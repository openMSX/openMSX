#ifndef GL_PROGRAMS_HH
#define GL_PROGRAMS_HH

#include "GLUtil.hh"

namespace gl {

// Initialize or destroy global openGL shader programs.
//  init()    must be called right after  the openGL context has been created
//  destroy() must be called right before the openGL context will be destroyed
void initPrograms();
void destroyPrograms();

// Simple texture program. It expects
//  uniforms:
//    unifTexColor: values from texture map are multiplied by this 4D color
//    unifTexMvp: Model-View-Projection-matrix
//  attributes:
//    0: 4D vertex positions, get multiplied by Model-View-Projection-matrix
//    1: 2D texture coordinates
//  textures:
//    the to be applied texture must be bound to the 1st texture unit
extern ShaderProgram progTex;
extern GLuint unifTexColor;
extern GLuint unifTexMvp;

} // namespace gl

#endif
