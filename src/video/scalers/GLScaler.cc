#include "GLScaler.hh"
#include "GLUtil.hh"
#include "GLPrograms.hh"

using namespace gl;

namespace openmsx {

GLScaler::GLScaler()
{
	VertexShader   vertexShader  ("superImpose.vert");
	FragmentShader fragmentShader("superImpose.frag");
	scalerProgram.attach(vertexShader);
	scalerProgram.attach(fragmentShader);
	scalerProgram.bindAttribLocation(0, "a_position");
	scalerProgram.bindAttribLocation(1, "a_texCoord");
	scalerProgram.link();

	scalerProgram.activate();
	glUniform1i(scalerProgram.getUniformLocation("tex"), 0);
	glUniform1i(scalerProgram.getUniformLocation("videoTex"), 1);
	glUniformMatrix4fv(scalerProgram.getUniformLocation("u_mvpMatrix"),
	                   1, GL_FALSE, &pixelMvp[0][0]);
}

GLScaler::~GLScaler()
{
}

void GLScaler::uploadBlock(
	unsigned /*srcStartY*/, unsigned /*srcEndY*/,
	unsigned /*lineWidth*/, FrameSource& /*paintFrame*/)
{
}

void GLScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned /*srcWidth*/,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	if (superImpose) {
		glActiveTexture(GL_TEXTURE1);
		superImpose->bind();
		glActiveTexture(GL_TEXTURE0);
		scalerProgram.activate();
	} else {
		progTex.activate();
		glUniform4f(unifTexColor, 1.0f, 1.0f, 1.0f, 1.0f);
		glUniformMatrix4fv(unifTexMvp, 1, GL_FALSE, &pixelMvp[0][0]);
	}

	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth);
}

void GLScaler::drawMultiTex(
	ColorTexture& src,
	unsigned srcStartY, unsigned srcEndY,
	float physSrcHeight, float logSrcHeight,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	bool textureFromZero)
{
	src.bind();
	// Note: hShift is pre-divided by srcWidth, while vShift will be divided
	//       by srcHeight later on.
	// Note: The coordinate is put just past zero, to avoid fract() in the
	//       fragment shader to wrap around on rounding errors.
	float hShift = textureFromZero ? 0.501f / dstWidth : 0.0f;
	float vShift = textureFromZero ? 0.501f * (
		float(srcEndY - srcStartY) / float(dstEndY - dstStartY)
		) : 0.0f;

	// vertex positions
	vec2 pos[4] = {
		vec2(       0, dstStartY),
		vec2(dstWidth, dstStartY),
		vec2(dstWidth, dstEndY  ),
		vec2(       0, dstEndY  ),
	};
	// texture coordinates, X-coord shared, Y-coord separate for tex0 and tex1
	float tex0StartY = (srcStartY + vShift) / physSrcHeight;
	float tex0EndY   = (srcEndY   + vShift) / physSrcHeight;
	float tex1StartY = (srcStartY + vShift) / logSrcHeight;
	float tex1EndY   = (srcEndY   + vShift) / logSrcHeight;
	vec3 tex[4] = {
		vec3(0.0f + hShift, tex0StartY, tex1StartY),
		vec3(1.0f + hShift, tex0StartY, tex1StartY),
		vec3(1.0f + hShift, tex0EndY  , tex1EndY  ),
		vec3(0.0f + hShift, tex0EndY  , tex1EndY  ),
	};

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, tex);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

} // namespace openmsx
