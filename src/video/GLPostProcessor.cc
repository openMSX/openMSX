#include "GLPostProcessor.hh"
#include "GLContext.hh"
#include "GLScaler.hh"
#include "GLScalerFactory.hh"
#include "FloatSetting.hh"
#include "OutputSurface.hh"
#include "RawFrame.hh"
#include "InitException.hh"
#include "gl_transform.hh"
#include "random.hh"
#include "ranges.hh"
#include "stl.hh"
#include "vla.hh"
#include "xrange.hh"
#include <cassert>
#include <cstdint>
#include <numeric>

using namespace gl;

namespace openmsx {

GLPostProcessor::GLPostProcessor(
	MSXMotherBoard& motherBoard_, Display& display_,
	OutputSurface& screen_, const std::string& videoSource,
	unsigned maxWidth_, unsigned height_, bool canDoInterlace_)
	: PostProcessor(motherBoard_, display_, screen_,
	                videoSource, maxWidth_, height_, canDoInterlace_)
	, noiseTextureA(true, true) // interpolate + wrap
	, noiseTextureB(true, true)
	, height(height_)
{
	scaleAlgorithm = RenderSettings::NO_SCALER;

	frameCounter = 0;
	noiseX = noiseY = 0.0f;
	preCalcNoise(renderSettings.getNoise());
	initBuffers();

	storedFrame = false;
	for (auto i : xrange(2)) {
		colorTex[i].bind();
		colorTex[i].setInterpolation(true);
		auto [w, h] = screen.getLogicalSize();
		glTexImage2D(GL_TEXTURE_2D,     // target
			     0,                 // level
			     GL_RGB,            // internal format
			     w,                 // width
			     h,                 // height
			     0,                 // border
			     GL_RGB,            // format
			     GL_UNSIGNED_BYTE,  // type
			     nullptr);          // data
		fbo[i] = FrameBufferObject(colorTex[i]);
	}

	VertexShader   vertexShader  ("monitor3D.vert");
	FragmentShader fragmentShader("monitor3D.frag");
	monitor3DProg.attach(vertexShader);
	monitor3DProg.attach(fragmentShader);
	monitor3DProg.bindAttribLocation(0, "a_position");
	monitor3DProg.bindAttribLocation(1, "a_normal");
	monitor3DProg.bindAttribLocation(2, "a_texCoord");
	monitor3DProg.link();
	preCalcMonitor3D(renderSettings.getHorizontalStretch());

	renderSettings.getNoiseSetting().attach(*this);
	renderSettings.getHorizontalStretchSetting().attach(*this);
}

GLPostProcessor::~GLPostProcessor()
{
	renderSettings.getHorizontalStretchSetting().detach(*this);
	renderSettings.getNoiseSetting().detach(*this);
}

void GLPostProcessor::initBuffers()
{
	// combined positions and texture coordinates
	static constexpr vec2 pos_tex[4 + 4] = {
		vec2(-1, 1), vec2(-1,-1), vec2( 1,-1), vec2( 1, 1), // pos
		vec2( 0, 1), vec2( 0, 0), vec2( 1, 0), vec2( 1, 1), // tex
	};
	glBindBuffer(GL_ARRAY_BUFFER, vbo.get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos_tex), pos_tex, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLPostProcessor::createRegions()
{
	regions.clear();

	const unsigned srcHeight = paintFrame->getHeight();
	const unsigned dstHeight = screen.getLogicalHeight();

	unsigned g = std::gcd(srcHeight, dstHeight);
	unsigned srcStep = srcHeight / g;
	unsigned dstStep = dstHeight / g;

	// TODO: Store all MSX lines in RawFrame and only scale the ones that fit
	//       on the PC screen, as a preparation for resizable output window.
	unsigned srcStartY = 0;
	unsigned dstStartY = 0;
	while (dstStartY < dstHeight) {
		// Currently this is true because the source frame height
		// is always >= dstHeight/(dstStep/srcStep).
		assert(srcStartY < srcHeight);

		// get region with equal lineWidth
		unsigned lineWidth = getLineWidth(paintFrame, srcStartY, srcStep);
		unsigned srcEndY = srcStartY + srcStep;
		unsigned dstEndY = dstStartY + dstStep;
		while ((srcEndY < srcHeight) && (dstEndY < dstHeight) &&
		       (getLineWidth(paintFrame, srcEndY, srcStep) == lineWidth)) {
			srcEndY += srcStep;
			dstEndY += dstStep;
		}

		regions.emplace_back(srcStartY, srcEndY,
		                     dstStartY, dstEndY,
		                     lineWidth);

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}
}

void GLPostProcessor::paint(OutputSurface& /*output*/)
{
	if (renderSettings.getInterleaveBlackFrame()) {
		interleaveCount ^= 1;
		if (interleaveCount) {
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			return;
		}
	}

	auto deform = renderSettings.getDisplayDeform();
	float horStretch = renderSettings.getHorizontalStretch();
	int glow = renderSettings.getGlow();
	bool renderToTexture = (deform != RenderSettings::DEFORM_NORMAL) ||
	                       (horStretch != 320.0f) ||
	                       (glow != 0) ||
	                       screen.isViewScaled();

	if ((screen.getViewOffset() != ivec2()) || // any part of the screen not covered by the viewport?
	    (deform == RenderSettings::DEFORM_3D) || !paintFrame) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		if (!paintFrame) {
			return;
		}
	}

	// New scaler algorithm selected?
	auto algo = renderSettings.getScaleAlgorithm();
	if (scaleAlgorithm != algo) {
		scaleAlgorithm = algo;
		currScaler = GLScalerFactory::createScaler(renderSettings);

		// Re-upload frame data, this is both
		//  - Chunks of RawFrame with a specific linewidth, possibly
		//    with some extra lines above and below each chunk that are
		//    also converted to this linewidth.
		//  - Extra data that is specific for the scaler (ATM only the
		//    hq and hqlite scalers require this).
		// Re-uploading the first is not strictly needed. But switching
		// scalers doesn't happen that often, so it also doesn't hurt
		// and it keeps the code simpler.
		uploadFrame();
	}

	auto [scrnWidth, scrnHeight] = screen.getLogicalSize();
	if (renderToTexture) {
		glViewport(0, 0, scrnWidth, scrnHeight);
		glBindTexture(GL_TEXTURE_2D, 0);
		fbo[frameCounter & 1].push();
	}

	for (auto& r : regions) {
		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	r.srcStartY, r.srcEndY, r.lineWidth);
		auto it = find_if_unguarded(textures,
		                  EqualTupleValue<0>(r.lineWidth));
		auto* superImpose = superImposeVideoFrame
		                  ? &superImposeTex : nullptr;
		currScaler->scaleImage(
			it->second.tex, superImpose,
			r.srcStartY, r.srcEndY, r.lineWidth, // src
			r.dstStartY, r.dstEndY, scrnWidth,   // dst
			paintFrame->getHeight()); // dst
	}

	drawNoise();
	drawGlow(glow);

	if (renderToTexture) {
		fbo[frameCounter & 1].pop();
		colorTex[frameCounter & 1].bind();
		auto [x, y] = screen.getViewOffset();
		auto [w, h] = screen.getViewSize();
		glViewport(x, y, w, h);

		if (deform == RenderSettings::DEFORM_3D) {
			drawMonitor3D();
		} else {
			float x1 = (320.0f - float(horStretch)) / (2.0f * 320.0f);
			float x2 = 1.0f - x1;
			vec2 tex[4] = {
				vec2(x1, 1), vec2(x1, 0), vec2(x2, 0), vec2(x2, 1)
			};

			gl::context->progTex.activate();
			glUniform4f(gl::context->unifTexColor,
			            1.0f, 1.0f, 1.0f, 1.0f);
			mat4 I;
			glUniformMatrix4fv(gl::context->unifTexMvp,
			                   1, GL_FALSE, &I[0][0]);

			glBindBuffer(GL_ARRAY_BUFFER, vbo.get());
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, stretchVBO.get());
			glBufferData(GL_ARRAY_BUFFER, sizeof(tex), tex, GL_STREAM_DRAW);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
			glEnableVertexAttribArray(1);

			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			glDisableVertexAttribArray(1);
			glDisableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		storedFrame = true;
	} else {
		storedFrame = false;
	}
	//gl::checkGLError("GLPostProcessor::paint");
}

std::unique_ptr<RawFrame> GLPostProcessor::rotateFrames(
	std::unique_ptr<RawFrame> finishedFrame, EmuTime::param time)
{
	std::unique_ptr<RawFrame> reuseFrame =
		PostProcessor::rotateFrames(std::move(finishedFrame), time);
	uploadFrame();
	++frameCounter;
	noiseX = random_float(0.0f, 1.0f);
	noiseY = random_float(0.0f, 1.0f);
	return reuseFrame;
}

void GLPostProcessor::update(const Setting& setting) noexcept
{
	VideoLayer::update(setting);
	auto& noiseSetting = renderSettings.getNoiseSetting();
	auto& horizontalStretch = renderSettings.getHorizontalStretchSetting();
	if (&setting == &noiseSetting) {
		preCalcNoise(noiseSetting.getDouble());
	} else if (&setting == &horizontalStretch) {
		preCalcMonitor3D(horizontalStretch.getDouble());
	}
}

void GLPostProcessor::uploadFrame()
{
	createRegions();

	const unsigned srcHeight = paintFrame->getHeight();
	for (auto& r : regions) {
		// upload data
		// TODO get before/after data from scaler
		unsigned before = 1;
		unsigned after  = 1;
		uploadBlock(std::max<int>(0,         r.srcStartY - before),
		            std::min<int>(srcHeight, r.srcEndY   + after),
		            r.lineWidth);
	}

	if (superImposeVideoFrame) {
		int w = superImposeVideoFrame->getWidth();
		int h = superImposeVideoFrame->getHeight();
		if (superImposeTex.getWidth()  != w ||
		    superImposeTex.getHeight() != h) {
			superImposeTex.resize(w, h);
			superImposeTex.setInterpolation(true);
		}
		superImposeTex.bind();
		glTexSubImage2D(
			GL_TEXTURE_2D,     // target
			0,                 // level
			0,                 // offset x
			0,                 // offset y
			w,                 // width
			h,                 // height
			GL_RGBA,           // format
			GL_UNSIGNED_BYTE,  // type
			const_cast<RawFrame*>(superImposeVideoFrame)->getLinePtrDirect<unsigned>(0)); // data
	}
}

void GLPostProcessor::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth)
{
	// create texture/pbo if needed
	auto it = ranges::find_if(textures, EqualTupleValue<0>(lineWidth));
	if (it == end(textures)) {
		TextureData textureData;

		textureData.tex.resize(lineWidth, height * 2); // *2 for interlace
		textureData.pbo.setImage(lineWidth, height * 2);
		textures.emplace_back(lineWidth, std::move(textureData));
		it = end(textures) - 1;
	}
	auto& tex = it->second.tex;
	auto& pbo = it->second.pbo;

	// bind texture
	tex.bind();

	// upload data
	pbo.bind();
	uint32_t* mapped = pbo.mapWrite();
	for (auto y : xrange(srcStartY, srcEndY)) {
		auto* dest = mapped + y * lineWidth;
		const auto* data = paintFrame->getLinePtr(y, lineWidth, dest);
		if (data != dest) {
			memcpy(dest, data, lineWidth * sizeof(uint32_t));
		}
	}
	pbo.unmap();
#if defined(__APPLE__)
	// The nVidia GL driver for the GeForce 8000/9000 series seems to hang
	// on texture data replacements that are 1 pixel wide and start on a
	// line number that is a non-zero multiple of 16.
	if (lineWidth == 1 && srcStartY != 0 && srcStartY % 16 == 0) {
		srcStartY--;
	}
#endif
	glTexSubImage2D(
		GL_TEXTURE_2D,       // target
		0,                   // level
		0,                   // offset x
		srcStartY,           // offset y
		lineWidth,           // width
		srcEndY - srcStartY, // height
		GL_RGBA,             // format
		GL_UNSIGNED_BYTE,    // type
		pbo.getOffset(0, srcStartY)); // data
	pbo.unbind();

	// possibly upload scaler specific data
	if (currScaler) {
		currScaler->uploadBlock(srcStartY, srcEndY, lineWidth, *paintFrame);
	}
}

void GLPostProcessor::drawGlow(int glow)
{
	if ((glow == 0) || !storedFrame) return;

	gl::context->progTex.activate();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	colorTex[(frameCounter & 1) ^ 1].bind();
	glUniform4f(gl::context->unifTexColor,
	            1.0f, 1.0f, 1.0f, glow * 31 / 3200.0f);
	mat4 I;
	glUniformMatrix4fv(gl::context->unifTexMvp, 1, GL_FALSE, &I[0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, vbo.get());

	const vec2* offset = nullptr;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, offset); // pos
	offset += 4; // see initBuffers()
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, offset); // tex
	glEnableVertexAttribArray(1);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisable(GL_BLEND);
}

void GLPostProcessor::preCalcNoise(float factor)
{
	GLbyte buf1[256 * 256];
	GLbyte buf2[256 * 256];
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::normal_distribution<float> distribution(0.0f, 1.0f);
	for (auto i : xrange(256 * 256)) {
		float r = distribution(generator);
		int s = std::clamp(int(roundf(r * factor)), -255, 255);
		buf1[i] = (s > 0) ?  s : 0;
		buf2[i] = (s < 0) ? -s : 0;
	}

	// GL_LUMINANCE is no longer supported in newer openGL versions
	auto format = (OPENGL_VERSION >= OPENGL_3_3) ? GL_RED : GL_LUMINANCE;
	noiseTextureA.bind();
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		format,           // internal format
		256,              // width
		256,              // height
		0,                // border
		format,           // format
		GL_UNSIGNED_BYTE, // type
		buf1);            // data
#if OPENGL_VERSION >= OPENGL_3_3
	GLint swizzleMask1[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask1);
#endif

	noiseTextureB.bind();
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		format,           // internal format
		256,              // width
		256,              // height
		0,                // border
		format,           // format
		GL_UNSIGNED_BYTE, // type
		buf2);            // data
#if OPENGL_VERSION >= OPENGL_3_3
	GLint swizzleMask2[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask2);
#endif
}

void GLPostProcessor::drawNoise()
{
	if (renderSettings.getNoise() == 0.0f) return;

	// Rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise.
	static constexpr vec2 pos[8][4] = {
		{ { -1, -1 }, {  1, -1 }, {  1,  1 }, { -1,  1 } },
		{ { -1,  1 }, {  1,  1 }, {  1, -1 }, { -1, -1 } },
		{ { -1,  1 }, { -1, -1 }, {  1, -1 }, {  1,  1 } },
		{ {  1,  1 }, {  1, -1 }, { -1, -1 }, { -1,  1 } },
		{ {  1,  1 }, { -1,  1 }, { -1, -1 }, {  1, -1 } },
		{ {  1, -1 }, { -1, -1 }, { -1,  1 }, {  1,  1 } },
		{ {  1, -1 }, {  1,  1 }, { -1,  1 }, { -1, -1 } },
		{ { -1, -1 }, { -1,  1 }, {  1,  1 }, {  1, -1 } }
	};
	vec2 noise(noiseX, noiseY);
	const vec2 tex[4] = {
		noise + vec2(0.0f, 1.875f),
		noise + vec2(2.0f, 1.875f),
		noise + vec2(2.0f, 0.0f  ),
		noise + vec2(0.0f, 0.0f  )
	};

	gl::context->progTex.activate();

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glUniform4f(gl::context->unifTexColor, 1.0f, 1.0f, 1.0f, 1.0f);
	mat4 I;
	glUniformMatrix4fv(gl::context->unifTexMvp, 1, GL_FALSE, &I[0][0]);

	unsigned seq = frameCounter & 7;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos[seq]);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	noiseTextureA.bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	noiseTextureB.bind();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBlendEquation(GL_FUNC_ADD); // restore default
	glDisable(GL_BLEND);
}

constexpr int GRID_SIZE = 16;
constexpr int GRID_SIZE1 = GRID_SIZE + 1;
constexpr int NUM_INDICES = (GRID_SIZE1 * 2 + 2) * GRID_SIZE - 2;
struct Vertex {
	vec3 position;
	vec3 normal;
	vec2 tex;
};

void GLPostProcessor::preCalcMonitor3D(float width)
{
	// precalculate vertex-positions, -normals and -texture-coordinates
	Vertex vertices[GRID_SIZE1][GRID_SIZE1];

	constexpr float GRID_SIZE2 = float(GRID_SIZE) / 2.0f;
	float s = width / 320.0f;
	float b = (320.0f - width) / (2.0f * 320.0f);

	for (auto sx : xrange(GRID_SIZE1)) {
		for (auto sy : xrange(GRID_SIZE1)) {
			Vertex& v = vertices[sx][sy];
			float x = (sx - GRID_SIZE2) / GRID_SIZE2;
			float y = (sy - GRID_SIZE2) / GRID_SIZE2;

			v.position = vec3(x, y, (x * x + y * y) / -12.0f);
			v.normal = normalize(vec3(x / 6.0f, y / 6.0f, 1.0f)) * 1.2f;
			v.tex = vec2((float(sx) / GRID_SIZE) * s + b,
			              float(sy) / GRID_SIZE);
		}
	}

	// calculate indices
	uint16_t indices[NUM_INDICES];

	uint16_t* ind = indices;
	for (auto y : xrange(GRID_SIZE)) {
		for (auto x : xrange(GRID_SIZE1)) {
			*ind++ = (y + 0) * GRID_SIZE1 + x;
			*ind++ = (y + 1) * GRID_SIZE1 + x;
		}
		// skip 2, filled in later
		ind += 2;
	}
	assert((ind - indices) == NUM_INDICES + 2);
	ind = indices;
	repeat(GRID_SIZE - 1, [&] {
		ind += 2 * GRID_SIZE1;
		// repeat prev and next index to restart strip
		ind[0] = ind[-1];
		ind[1] = ind[ 2];
		ind += 2;
	});

	// upload calculated values to buffers
	glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer.get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer.get());
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// calculate transformation matrices
	mat4 proj = frustum(-1, 1, -1, 1, 1, 10);
	mat4 tran = translate(vec3(0.0f, 0.4f, -2.0f));
	mat4 rotX = rotateX(radians(-10.0f));
	mat4 scal = scale(vec3(2.2f, 2.2f, 2.2f));

	mat3 normal = mat3(rotX);
	mat4 mvp = proj * tran * rotX * scal;

	// set uniforms
	monitor3DProg.activate();
	glUniform1i(monitor3DProg.getUniformLocation("u_tex"), 0);
	glUniformMatrix4fv(monitor3DProg.getUniformLocation("u_mvpMatrix"),
		1, GL_FALSE, &mvp[0][0]);
	glUniformMatrix3fv(monitor3DProg.getUniformLocation("u_normalMatrix"),
		1, GL_FALSE, &normal[0][0]);
}

void GLPostProcessor::drawMonitor3D()
{
	monitor3DProg.activate();

	char* base = nullptr;
	glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer.get());
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer.get());
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
	                      base);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
	                      base + sizeof(vec3));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
	                      base + sizeof(vec3) + sizeof(vec3));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glDrawElements(GL_TRIANGLE_STRIP, NUM_INDICES, GL_UNSIGNED_SHORT, nullptr);
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace openmsx
