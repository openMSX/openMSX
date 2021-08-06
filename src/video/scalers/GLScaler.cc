#include "GLScaler.hh"
#include "GLContext.hh"
#include "gl_vec.hh"
#include "strCat.hh"
#include "xrange.hh"

using namespace gl;

namespace openmsx {

GLScaler::GLScaler(const std::string& progName)
{
	for (auto i : xrange(2)) {
		auto header = tmpStrCat("#define SUPERIMPOSE ", char('0' + i), '\n');
		VertexShader   vShader(header, progName + ".vert");
		FragmentShader fShader(header, progName + ".frag");
		program[i].attach(vShader);
		program[i].attach(fShader);
		program[i].bindAttribLocation(0, "a_position");
		program[i].bindAttribLocation(1, "a_texCoord");
		program[i].link();
		program[i].activate();
		glUniform1i(program[i].getUniformLocation("tex"), 0);
		if (i == 1) {
			glUniform1i(program[i].getUniformLocation("videoTex"), 1);
		}
		unifTexSize[i] = program[i].getUniformLocation("texSize");
		glUniformMatrix4fv(program[i].getUniformLocation("u_mvpMatrix"),
		                   1, GL_FALSE, &gl::context->pixelMvp[0][0]);
	}
}

void GLScaler::uploadBlock(
	unsigned /*srcStartY*/, unsigned /*srcEndY*/,
	unsigned /*lineWidth*/, FrameSource& /*paintFrame*/)
{
}

void GLScaler::setup(bool superImpose)
{
	int i = superImpose ? 1 : 0;
	program[i].activate();
}

void GLScaler::execute(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight, bool textureFromZero)
{
	if (superImpose) {
		glActiveTexture(GL_TEXTURE1);
		superImpose->bind();
		glActiveTexture(GL_TEXTURE0);
	}
	int i = superImpose ? 1 : 0;
	glUniform3f(unifTexSize[i], srcWidth, src.getHeight(), logSrcHeight);

	src.bind();
	// Note: hShift is pre-divided by srcWidth, while vShift will be divided
	//       by srcHeight later on.
	// Note: The coordinate is put just past zero, to avoid fract() in the
	//       fragment shader to wrap around on rounding errors.
	constexpr float BIAS = 0.001f;
	float samplePos = (textureFromZero ? 0.5f : 0.0f) + BIAS;
	float hShift = samplePos / dstWidth;
	float yRatio = float(srcEndY - srcStartY) / float(dstEndY - dstStartY);
	float vShift = samplePos * yRatio;

	// vertex positions
	vec2 pos[4] = {
		vec2(       0, dstStartY),
		vec2(dstWidth, dstStartY),
		vec2(dstWidth, dstEndY  ),
		vec2(       0, dstEndY  ),
	};
	// texture coordinates, X-coord shared, Y-coord separate for tex0 and tex1
	float tex0StartY = float(srcStartY + vShift) / src.getHeight();
	float tex0EndY   = float(srcEndY   + vShift) / src.getHeight();
	float tex1StartY = float(srcStartY + vShift) / logSrcHeight;
	float tex1EndY   = float(srcEndY   + vShift) / logSrcHeight;
	vec3 tex[4] = {
		vec3(0.0f + hShift, tex0StartY, tex1StartY),
		vec3(1.0f + hShift, tex0StartY, tex1StartY),
		vec3(1.0f + hShift, tex0EndY  , tex1EndY  ),
		vec3(0.0f + hShift, tex0EndY  , tex1EndY  ),
	};

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0].get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STREAM_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1].get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(tex), tex, GL_STREAM_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace openmsx
