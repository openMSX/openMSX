#include "GLSnow.hh"
#include "GLContext.hh"
#include "gl_mat.hh"
#include "Display.hh"
#include "gl_vec.hh"
#include "openmsx.hh"
#include "random.hh"

using namespace gl;

namespace openmsx {

GLSnow::GLSnow(Display& display_)
	: Layer(COVER_FULL, Z_BACKGROUND)
	, display(display_)
	, noiseTexture(true, true) // enable interpolation + wrapping
{
	// Create noise texture.
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::uniform_int_distribution<int> distribution(0, 255);
	byte buf[128 * 128];
	for (auto& b : buf) {
		b = distribution(generator);
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 128, 128, 0,
	             GL_RED, GL_UNSIGNED_BYTE, buf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

	const vec2 pos[8][4] = {
		{ { -1, -1 }, {  1, -1 }, {  1,  1 }, { -1,  1 } },
		{ { -1,  1 }, {  1,  1 }, {  1, -1 }, { -1, -1 } },
		{ { -1,  1 }, { -1, -1 }, {  1, -1 }, {  1,  1 } },
		{ {  1,  1 }, {  1, -1 }, { -1, -1 }, { -1,  1 } },
		{ {  1,  1 }, { -1,  1 }, { -1, -1 }, {  1, -1 } },
		{ {  1, -1 }, { -1, -1 }, { -1,  1 }, {  1,  1 } },
		{ {  1, -1 }, {  1,  1 }, { -1,  1 }, { -1, -1 } },
		{ { -1, -1 }, { -1,  1 }, {  1,  1 }, {  1, -1 } }
	};
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0].get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);

}

void GLSnow::paint(OutputSurface& /*output*/)
{
	// Rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise.
	static unsigned cnt = 0;
	cnt = (cnt + 1) % 8;

	vec2 offset(random_float(0.0f, 1.0f) ,random_float(0.0f, 1.0f));
	const vec2 tex[4] = {
		offset + vec2(0.0f, 2.0f),
		offset + vec2(2.0f, 2.0f),
		offset + vec2(2.0f, 0.0f),
		offset + vec2(0.0f, 0.0f)
	};

	gl::context->progTex.activate();
	glUniform4f(gl::context->unifTexColor, 1.0f, 1.0f, 1.0f, 1.0f);
	mat4 I;
	glUniformMatrix4fv(gl::context->unifTexMvp, 1, GL_FALSE, &I[0][0]);

	vao.bind();
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0].get());
	vec2* base = nullptr;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, base + cnt * 4);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[1].get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(tex), tex, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(1);
	noiseTexture.bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	vao.unbind();

	display.repaintDelayed(100 * 1000); // 10fps
}

} // namespace openmsx
