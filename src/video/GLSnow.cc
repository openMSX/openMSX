#include "GLSnow.hh"
#include "Display.hh"
#include "gl_vec.hh"
#include "openmsx.hh"
#include <cstdlib>

using namespace gl;

namespace openmsx {

GLSnow::GLSnow(Display& display_)
	: Layer(COVER_FULL, Z_BACKGROUND)
	, display(display_)
	, noiseTexture(true) // enable interpolation
{
	// Create noise texture.
	byte buf[128 * 128];
	for (auto& b : buf) {
		b = byte(rand());
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 128, 128, 0,
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, buf);

	// TODO The exact same program is used in GLPostProcessor.
	//      Can we share them?
	VertexShader   texVertexShader  ("tex2D.vert");
	FragmentShader texFragmentShader("tex2D.frag");
	texProg.attach(texVertexShader);
	texProg.attach(texFragmentShader);
	texProg.bindAttribLocation(0, "a_position");
	texProg.bindAttribLocation(1, "a_texCoord");
	texProg.link();
	texProg.activate();
	glUniform1i(texProg.getUniformLocation("u_tex"),  0);
	texProg.deactivate();
}

void GLSnow::paint(OutputSurface& /*output*/)
{
	// Rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise.
	static const vec2 pos[8][4] = {
		{ { -1, -1 }, {  1, -1 }, {  1,  1 }, { -1,  1 } },
		{ { -1,  1 }, {  1,  1 }, {  1, -1 }, { -1, -1 } },
		{ { -1,  1 }, { -1, -1 }, {  1, -1 }, {  1,  1 } },
		{ {  1,  1 }, {  1, -1 }, { -1, -1 }, { -1,  1 } },
		{ {  1,  1 }, { -1,  1 }, { -1, -1 }, {  1, -1 } },
		{ {  1, -1 }, { -1, -1 }, { -1,  1 }, {  1,  1 } },
		{ {  1, -1 }, {  1,  1 }, { -1,  1 }, { -1, -1 } },
		{ { -1, -1 }, { -1,  1 }, {  1,  1 }, {  1, -1 } }
	};
	static unsigned cnt = 0;
	cnt = (cnt + 1) % 8;

	vec2 offset(float(rand()) / RAND_MAX, float(rand()) / RAND_MAX);
	const vec2 tex[4] = {
		offset + vec2(0.0f, 2.0f),
		offset + vec2(2.0f, 2.0f),
		offset + vec2(2.0f, 0.0f),
		offset + vec2(0.0f, 0.0f)
	};

	texProg.activate();
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos[cnt]);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	noiseTexture.bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	texProg.deactivate();

	display.repaintDelayed(100 * 1000); // 10fps
}

} // namespace openmsx
