#include "GLPostProcessor.hh"
#include "GLContext.hh"
#include "GLScaler.hh"
#include "GLScalerFactory.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "OutputSurface.hh"
#include "RawFrame.hh"
#include "Math.hh"
#include "InitException.hh"
#include "gl_transform.hh"
#include "random.hh"
#include "stl.hh"
#include "vla.hh"
#include <algorithm>
#include <cassert>

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
	if (!glewIsSupported("GL_EXT_framebuffer_object")) {
		throw InitException(
			"The OpenGL framebuffer object is not supported by "
			"this glew library. Please upgrade your glew library.\n"
			"It's also possible (but less likely) your video card "
			"or video card driver doesn't support framebuffer "
			"objects.");
	}

	scaleAlgorithm = static_cast<RenderSettings::ScaleAlgorithm>(-1); // not a valid scaler

	frameCounter = 0;
	noiseX = noiseY = 0.0f;
	preCalcNoise(renderSettings.getNoise());

	storedFrame = false;
	for (int i = 0; i < 2; ++i) {
		colorTex[i].bind();
		colorTex[i].setInterpolation(true);
		glTexImage2D(GL_TEXTURE_2D,     // target
			     0,                 // level
			     GL_RGB8,           // internal format
			     screen.getWidth(), // width
			     screen.getHeight(),// height
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

void GLPostProcessor::createRegions()
{
	regions.clear();

	const unsigned srcHeight = paintFrame->getHeight();
	const unsigned dstHeight = screen.getHeight();

	unsigned g = Math::gcd(srcHeight, dstHeight);
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
	                       (glow != 0);

	if ((deform == RenderSettings::DEFORM_3D) || !paintFrame) {
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

	if (renderToTexture) {
		glViewport(0, 0, screen.getWidth(), screen.getHeight());
		glBindTexture(GL_TEXTURE_2D, 0);
		fbo[frameCounter & 1].push();
	}

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (auto& r : regions) {
		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	r.srcStartY, r.srcEndY, r.lineWidth);
		auto it = find_if_unguarded(textures,
		                  EqualTupleValue<0>(r.lineWidth));
		auto superImpose = superImposeVideoFrame
		                 ? &superImposeTex : nullptr;
		currScaler->scaleImage(
			it->second.tex, superImpose,
			r.srcStartY, r.srcEndY, r.lineWidth,       // src
			r.dstStartY, r.dstEndY, screen.getWidth(), // dst
			paintFrame->getHeight()); // dst
		//GLUtil::checkGLError("GLPostProcessor::paint");
	}

	drawNoise();
	drawGlow(glow);

	if (renderToTexture) {
		fbo[frameCounter & 1].pop();
		colorTex[frameCounter & 1].bind();
		glViewport(screen.getX(), screen.getY(),
		           screen.getWidth(), screen.getHeight());

		if (deform == RenderSettings::DEFORM_3D) {
			drawMonitor3D();
		} else {
			float x1 = (320.0f - float(horStretch)) / (2.0f * 320.0f);
			float x2 = 1.0f - x1;

			static const vec2 pos[4] = {
				vec2(-1, 1), vec2(-1,-1), vec2( 1,-1), vec2( 1, 1)
			};
			vec2 tex[4] = {
				vec2(x1, 1), vec2(x1, 0), vec2(x2, 0), vec2(x2, 1)
			};

			gl::context->progTex.activate();
			glUniform4f(gl::context->unifTexColor,
			            1.0f, 1.0f, 1.0f, 1.0f);
			mat4 I;
			glUniformMatrix4fv(gl::context->unifTexMvp,
			                   1, GL_FALSE, &I[0][0]);
			glDisable(GL_BLEND);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex);
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
		storedFrame = true;
	} else {
		storedFrame = false;
	}
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

void GLPostProcessor::update(const Setting& setting)
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
			GL_BGRA,           // format
			GL_UNSIGNED_BYTE,  // type
			const_cast<RawFrame*>(superImposeVideoFrame)->getLinePtrDirect<unsigned>(0)); // data
	}
}

void GLPostProcessor::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth)
{
	// create texture/pbo if needed
	auto it = find_if(begin(textures), end(textures),
	                  EqualTupleValue<0>(lineWidth));
	if (it == end(textures)) {
		TextureData textureData;

		textureData.tex.resize(lineWidth, height * 2); // *2 for interlace

		if (textureData.pbo.openGLSupported()) {
			textureData.pbo.setImage(lineWidth, height * 2);
		}

		textures.emplace_back(lineWidth, std::move(textureData));
		it = end(textures) - 1;
	}
	auto& tex = it->second.tex;
	auto& pbo = it->second.pbo;

	// bind texture
	tex.bind();

	// upload data
	uint32_t* mapped;
	if (pbo.openGLSupported()) {
		pbo.bind();
		mapped = pbo.mapWrite();
	} else {
		mapped = nullptr;
	}
	if (mapped) {
		for (unsigned y = srcStartY; y < srcEndY; ++y) {
			auto* dest = mapped + y * lineWidth;
			auto* data = paintFrame->getLinePtr(y, lineWidth, dest);
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
			GL_BGRA,             // format
			GL_UNSIGNED_BYTE,    // type
			pbo.getOffset(0, srcStartY)); // data
	}
	if (pbo.openGLSupported()) {
		pbo.unbind();
	}
	if (!mapped) {
		glPixelStorei(GL_UNPACK_ROW_LENGTH, paintFrame->getRowLength());
		unsigned y = srcStartY;
		unsigned remainingLines = srcEndY - srcStartY;
		VLA_SSE_ALIGNED(uint32_t, buf, lineWidth);
		while (remainingLines) {
			unsigned lines;
			auto* data = paintFrame->getMultiLinePtr(
				y, remainingLines, lines, lineWidth, buf);
			glTexSubImage2D(
				GL_TEXTURE_2D,     // target
				0,                 // level
				0,                 // offset x
				y,                 // offset y
				lineWidth,         // width
				lines,             // height
				GL_BGRA,           // format
				GL_UNSIGNED_BYTE,  // type
				data);             // data

			y += lines;
			remainingLines -= lines;
		}
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // restore default
	}

	// possibly upload scaler specific data
	if (currScaler) {
		currScaler->uploadBlock(srcStartY, srcEndY, lineWidth, *paintFrame);
	}
}

void GLPostProcessor::drawGlow(int glow)
{
	if ((glow == 0) || !storedFrame) return;

	static const vec2 pos[4] = {
		vec2(-1, 1), vec2(-1,-1), vec2( 1,-1), vec2( 1, 1)
	};
	static const vec2 tex[4] = {
		vec2( 0, 1), vec2( 0, 0), vec2( 1, 0), vec2( 1, 1)
	};

	gl::context->progTex.activate();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	colorTex[(frameCounter & 1) ^ 1].bind();
	glUniform4f(gl::context->unifTexColor,
	            1.0f, 1.0f, 1.0f, glow * 31 / 3200.0f);
	mat4 I;
	glUniformMatrix4fv(gl::context->unifTexMvp, 1, GL_FALSE, &I[0][0]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisable(GL_BLEND);
}

void GLPostProcessor::preCalcNoise(float factor)
{
	GLbyte buf1[256 * 256];
	GLbyte buf2[256 * 256];
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::normal_distribution<float> distribution(0.0f, 1.0f);
	for (int i = 0; i < 256 * 256; ++i) {
		float r = distribution(generator);
		int s = Math::clip<-255, 255>(roundf(r * factor));
		buf1[i] = (s > 0) ?  s : 0;
		buf2[i] = (s < 0) ? -s : 0;
	}

	noiseTextureA.bind();
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		GL_LUMINANCE,     // internal format
		256,              // width
		256,              // height
		0,                // border
		GL_LUMINANCE,     // format
		GL_UNSIGNED_BYTE, // type
		buf1);            // data

	noiseTextureB.bind();
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		GL_LUMINANCE,     // internal format
		256,              // width
		256,              // height
		0,                // border
		GL_LUMINANCE,     // format
		GL_UNSIGNED_BYTE, // type
		buf2);            // data
}

void GLPostProcessor::drawNoise()
{
	if (renderSettings.getNoise() == 0.0f) return;

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
	glBlendEquation(GL_FUNC_ADD); // restore default
}

static const int GRID_SIZE = 16;
static const int GRID_SIZE1 = GRID_SIZE + 1;
static const int NUM_INDICES = (GRID_SIZE1 * 2 + 2) * GRID_SIZE - 2;
struct Vertex {
	vec3 position;
	vec3 normal;
	vec2 tex;
};

void GLPostProcessor::preCalcMonitor3D(float width)
{
	// precalculate vertex-positions, -normals and -texture-coordinates
	Vertex vertices[GRID_SIZE1][GRID_SIZE1];

	static const float GRID_SIZE2 = float(GRID_SIZE) / 2.0f;
	float s = width / 320.0f;
	float b = (320.0f - width) / (2.0f * 320.0f);

	for (int sx = 0; sx < GRID_SIZE1; ++sx) {
		for (int sy = 0; sy < GRID_SIZE1; ++sy) {
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
	unsigned short indices[NUM_INDICES];

	unsigned short* ind = indices;
	for (int y = 0; y < GRID_SIZE; ++y) {
		for (int x = 0; x < GRID_SIZE1; ++x) {
			*ind++ = (y + 0) * GRID_SIZE1 + x;
			*ind++ = (y + 1) * GRID_SIZE1 + x;
		}
		// skip 2, filled in later
		ind += 2;
	}
	assert((ind - indices) == NUM_INDICES + 2);
	ind = indices;
	for (int y = 0; y < (GRID_SIZE - 1); ++y) {
		ind += 2 * GRID_SIZE1;
		// repeat prev and next index to restart strip
		ind[0] = ind[-1];
		ind[1] = ind[ 2];
		ind += 2;
	}

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
	mat4 rotx = rotateX(radians(-10.0f));
	mat4 scal = scale(vec3(2.2f, 2.2f, 2.2f));

	mat3 normal = mat3(rotx);
	mat4 mvp = proj * tran * rotx * scal;

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

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace openmsx
