#include "GLPrograms.hh"
#include "gl_transform.hh"

namespace gl {

// Global variables
ShaderProgram progTex((Null()));
GLuint unifTexColor = -1;
GLuint unifTexMvp   = -1;

ShaderProgram progFill((Null()));
GLuint unifFillMvp  = -1;

mat4 pixelMvp;


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
	progTex.deactivate();

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
	progFill.deactivate();

	pixelMvp = ortho(0, width, height, 0, -1, 1);
}

void destroyPrograms()
{
	progTex.reset();
	unifTexColor = -1;
	unifTexMvp   = -1;

	progFill.reset();
	unifFillMvp = -1;
}

} // namespace gl
