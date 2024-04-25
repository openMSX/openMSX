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
	: Layer(Coverage::FULL, Z_BACKGROUND)
	, display(display_)
	, noiseTexture(true, true) // enable interpolation + wrapping
{
	// Create noise texture.
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::uniform_int_distribution distribution(0, 255);
	std::array<uint8_t, 128 * 128> buf;
	ranges::generate(buf, [&] { return distribution(generator); });
#if OPENGL_VERSION < OPENGL_3_3
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 128, 128, 0,
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, buf.data());
#else
	// GL_LUMINANCE no longer supported in newer versions
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 128, 128, 0,
	             GL_RED, GL_UNSIGNED_BYTE, buf.data());
	GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
#endif

	static constexpr std::array pos = {
		std::array{vec2{-1, -1}, vec2{ 1, -1}, vec2{ 1,  1}, vec2{-1,  1}},
		std::array{vec2{-1,  1}, vec2{ 1,  1}, vec2{ 1, -1}, vec2{-1, -1}},
		std::array{vec2{-1,  1}, vec2{-1, -1}, vec2{ 1, -1}, vec2{ 1,  1}},
		std::array{vec2{ 1,  1}, vec2{ 1, -1}, vec2{-1, -1}, vec2{-1,  1}},
		std::array{vec2{ 1,  1}, vec2{-1,  1}, vec2{-1, -1}, vec2{ 1, -1}},
		std::array{vec2{ 1, -1}, vec2{-1, -1}, vec2{-1,  1}, vec2{ 1,  1}},
		std::array{vec2{ 1, -1}, vec2{ 1,  1}, vec2{-1,  1}, vec2{-1, -1}},
		std::array{vec2{-1, -1}, vec2{-1,  1}, vec2{ 1,  1}, vec2{ 1, -1}},
	};
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0].get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLSnow::paint(OutputSurface& /*output*/)
{
	// Rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise.
	static unsigned cnt = 0;
	cnt = (cnt + 1) % 8;

	vec2 offset(random_float(0.0f, 1.0f), random_float(0.0f, 1.0f));
	const std::array tex = {
		offset + vec2(0.0f, 2.0f),
		offset + vec2(2.0f, 2.0f),
		offset + vec2(2.0f, 0.0f),
		offset + vec2(0.0f, 0.0f),
	};

	auto& glContext = *gl::context;
	glContext.progTex.activate();
	glUniform4f(glContext.unifTexColor, 1.0f, 1.0f, 1.0f, 1.0f);
	mat4 I;
	glUniformMatrix4fv(glContext.unifTexMvp, 1, GL_FALSE, I.data());

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0].get());
	const vec2* base = nullptr;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, base + 4 * cnt);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1].get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(tex), tex.data(), GL_STREAM_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(1);

	noiseTexture.bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	display.repaintDelayed(100 * 1000); // 10fps
}

} // namespace openmsx
