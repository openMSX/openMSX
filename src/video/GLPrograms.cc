#include "GLPrograms.hh"

namespace gl {

// Global variables
ShaderProgram progTex((Null()));
GLuint unifTexColor = -1;
GLuint unifTexMvp   = -1;


void initPrograms()
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
}

void destroyPrograms()
{
	progTex.reset();
	unifTexColor = -1;
	unifTexMvp   = -1;
}

} // namespace gl
