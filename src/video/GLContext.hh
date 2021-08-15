#ifndef GL_CONTEXT_HH
#define GL_CONTEXT_HH

#include "GLUtil.hh"
#include "gl_mat.hh"
#include <memory>
#include <optional>

namespace openmsx { class GLScaler; }

namespace gl {

struct Context
{
	/** Initialize global openGL state
	 */
	Context(int width, int height);
	~Context();

	// Simple texture program. It expects
	//  uniforms:
	//    unifTexColor: values from texture map are multiplied by this 4D color
	//    unifTexMvp: Model-View-Projection-matrix
	//  attributes:
	//    0: 4D vertex positions, get multiplied by Model-View-Projection-matrix
	//    1: 2D texture coordinates
	//  textures:
	//    the to be applied texture must be bound to the 1st texture unit
	ShaderProgram progTex;
	GLuint unifTexColor;
	GLuint unifTexMvp;

	// Simple color-fill program. It expects
	//  uniforms:
	//    unifFillMvp: Model-View-Projection-matrix
	//  attributes:
	//    0: 4D vertex positions, get multiplied by Model-View-Projection-matrix
	//    1: 4D vertex color
	ShaderProgram progFill;
	GLuint unifFillMvp;

	// Model-View-Projection-matrix that maps integer vertex positions to host
	// display pixel positions. (0,0) is the top-left pixel, (width-1,height-1) is
	// the bottom-right pixel.
	mat4 pixelMvp;

	// Fallback scaler
	openmsx::GLScaler& getFallbackScaler();

private:
	std::unique_ptr<openmsx::GLScaler> fallbackScaler;

};

extern std::optional<Context> context;

} // namespace gl

#endif
