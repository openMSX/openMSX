#ifndef GL_PROGRAMS_HH
#define GL_PROGRAMS_HH

#include "GLUtil.hh"
#include "gl_mat.hh"
#include <memory>

namespace openmsx { class GLScaler; }

namespace gl {

// Initialize or destroy global openGL shader programs.
//  init()    must be called right after  the openGL context has been created
//  destroy() must be called right before the openGL context will be destroyed
void initPrograms(int width, int height);
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

// Simple color-fill program. It expects
//  uniforms:
//    unifFillMvp: Model-View-Projection-matrix
//  attributes:
//    0: 4D vertex positions, get multiplied by Model-View-Projection-matrix
//    1: 4D vertex color
extern ShaderProgram progFill;
extern GLuint unifFillMvp;

// Model-View-Projection-matrix that maps integer vertex positions to host
// display pixel positions. (0,0) is the top-left pixel, (width-1,height-1) is
// the bottom-right pixel.
extern mat4 pixelMvp;

// Fallback scaler
extern std::unique_ptr<openmsx::GLScaler> fallbackScaler;

} // namespace gl

#endif
