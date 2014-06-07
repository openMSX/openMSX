#include "GLPrograms.hh"
#include "GLDefaultScaler.hh"
#include "gl_transform.hh"
#include "memory.hh"

namespace gl {

// Global variables
ShaderProgram progTex((Null()));
GLuint unifTexColor = -1;
GLuint unifTexMvp   = -1;

ShaderProgram progFill((Null()));
GLuint unifFillMvp  = -1;

mat4 pixelMvp;

std::unique_ptr<openmsx::GLScaler> fallbackScaler;


void initPrograms(int width, int height)
{
	VertexShader   texVertexShader  ("texture.vert");
	FragmentShader texFragmentShader("texture.frag");
	progTex.allocate();
	progTex.attach(texVertexShader);
	progTex.attach(texFragmentShader);
	progTex.bindAttribLocation(0, "a_position");
	progTex.bindAttribLocation(1, "a_texCoord");
	progTex.link();
	progTex.activate();
	glUniform1i(progTex.getUniformLocation("u_tex"), 0);
	unifTexColor = progTex.getUniformLocation("u_color");
	unifTexMvp   = progTex.getUniformLocation("u_mvpMatrix");

	VertexShader   fillVertexShader  ("fill.vert");
	FragmentShader fillFragmentShader("fill.frag");
	progFill.allocate();
	progFill.attach(fillVertexShader);
	progFill.attach(fillFragmentShader);
	progFill.bindAttribLocation(0, "a_position");
	progFill.bindAttribLocation(1, "a_color");
	progFill.link();
	progFill.activate();
	unifFillMvp = progFill.getUniformLocation("u_mvpMatrix");

	pixelMvp = ortho(0, width, height, 0, -1, 1);

	fallbackScaler = make_unique<openmsx::GLDefaultScaler>();
}

void destroyPrograms()
{
	progTex.reset();
	unifTexColor = -1;
	unifTexMvp   = -1;

	progFill.reset();
	unifFillMvp = -1;

	fallbackScaler.reset();
}

} // namespace gl
