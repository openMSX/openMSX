#include "PostProcessor.hh"

#include "AviRecorder.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "Deflicker.hh"
#include "DeinterlacedFrame.hh"
#include "Display.hh"
#include "DoubledFrame.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "FloatSetting.hh"
#include "GLContext.hh"
#include "GLScaler.hh"
#include "GLScalerFactory.hh"
#include "MSXMotherBoard.hh"
#include "OutputSurface.hh"
#include "PNG.hh"
#include "RawFrame.hh"
#include "Reactor.hh"
#include "RenderSettings.hh"
#include "SuperImposedFrame.hh"
#include "gl_transform.hh"

#include "MemBuffer.hh"
#include "aligned.hh"
#include "inplace_buffer.hh"
#include "narrow.hh"
#include "random.hh"
#include "ranges.hh"
#include "stl.hh"
#include "xrange.hh"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <numeric>

using namespace gl;

namespace openmsx {

PostProcessor::PostProcessor(
	MSXMotherBoard& motherBoard_, Display& display_,
	OutputSurface& screen_, const std::string& videoSource,
	unsigned maxWidth_, unsigned height_, bool canDoInterlace_)
	: VideoLayer(motherBoard_, videoSource)
	, Schedulable(motherBoard_.getScheduler())
	, display(display_)
	, renderSettings(display_.getRenderSettings())
	, eventDistributor(motherBoard_.getReactor().getEventDistributor())
	, screen(screen_)
	, maxWidth(maxWidth_)
	, height(height_)
	, canDoInterlace(canDoInterlace_)
	, lastRotate(motherBoard_.getCurrentTime())
{
	if (canDoInterlace) {
		deinterlacedFrame = std::make_unique<DeinterlacedFrame>();
		interlacedFrame   = std::make_unique<DoubledFrame>();
		deflicker = std::make_unique<Deflicker>(lastFrames);
		superImposedFrame = std::make_unique<SuperImposedFrame>();
	} else {
		// Laserdisc always produces non-interlaced frames, so we don't
		// need lastFrames[1..3], deinterlacedFrame and
		// interlacedFrame. Also it produces a complete frame at a
		// time, so we don't need lastFrames[0] (and have a separate
		// work buffer, for partially rendered frames).
	}

	preCalcNoise(renderSettings.getNoise());
	initBuffers();

	initAreaScaler();

	VertexShader   vertexShader  ("monitor3D.vert");
	FragmentShader fragmentShader("monitor3D.frag");
	monitor3DProg.attach(vertexShader);
	monitor3DProg.attach(fragmentShader);
	monitor3DProg.bindAttribLocation(0, "a_position");
	monitor3DProg.bindAttribLocation(1, "a_normal");
	monitor3DProg.bindAttribLocation(2, "a_texCoord");
	monitor3DProg.link();
	preCalcMonitor3D(renderSettings.getHorizontalStretch());

	pbo.allocate(maxWidth * height * 2); // *2 for interlace    TODO only when 'canDoInterlace'

	renderSettings.getNoiseSetting().attach(*this);
	renderSettings.getHorizontalStretchSetting().attach(*this);
}

PostProcessor::~PostProcessor()
{
	renderSettings.getHorizontalStretchSetting().detach(*this);
	renderSettings.getNoiseSetting().detach(*this);

	if (recorder) {
		getCliComm().printWarning(
			"Video recording stopped, because you "
			"changed machine or changed a video setting "
			"during recording.");
		recorder->stop();
	}
}

void PostProcessor::initBuffers()
{
	// combined positions and texture coordinates
	static constexpr std::array pos_tex = {
		vec2(-1, 1), vec2(-1,-1), vec2( 1,-1), vec2( 1, 1), // pos
		vec2( 0, 1), vec2( 0, 0), vec2( 1, 0), vec2( 1, 1), // tex
	};
	glBindBuffer(GL_ARRAY_BUFFER, vbo.get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos_tex), pos_tex.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

CliComm& PostProcessor::getCliComm()
{
	return display.getCliComm();
}

void PostProcessor::executeUntil(EmuTime::param /*time*/)
{
	// insert fake end of frame event
	eventDistributor.distributeEvent(FinishFrameEvent(
		getVideoSource(), getVideoSourceSetting(), false));
}

using WorkBuffer = std::vector<MemBuffer<FrameSource::Pixel, SSE_ALIGNMENT>>;
static void getScaledFrame(const FrameSource& paintFrame,
                           std::span<const FrameSource::Pixel*> lines,
                           WorkBuffer& workBuffer)
{
	auto height = narrow<unsigned>(lines.size());
	unsigned width = (height == 240) ? 320 : 640;
	const FrameSource::Pixel* linePtr = nullptr;
	FrameSource::Pixel* work = nullptr;
	for (auto i : xrange(height)) {
		if (linePtr == work) {
			// If work buffer was used in previous iteration,
			// then allocate a new one.
			work = workBuffer.emplace_back(width).data();
		}
		if (height == 240) {
			auto line = paintFrame.getLinePtr320_240(i, std::span<uint32_t, 320>{work, 320});
			linePtr = line.data();
		} else {
			assert (height == 480);
			auto line = paintFrame.getLinePtr640_480(i, std::span<uint32_t, 640>{work, 640});
			linePtr = line.data();
		}
		lines[i] = linePtr;
	}
}

void PostProcessor::takeRawScreenShot(unsigned height2, const std::string& filename)
{
	if (!paintFrame) {
		throw CommandException("TODO");
	}

	inplace_buffer<const FrameSource::Pixel*, 480> lines(uninitialized_tag{}, height2);
	WorkBuffer workBuffer;
	getScaledFrame(*paintFrame, lines, workBuffer);
	unsigned width = (height2 == 240) ? 320 : 640;
	PNG::saveRGBA(width, lines, filename);
}

void PostProcessor::createRegions()
{
	regions.clear();

	const unsigned srcHeight = paintFrame->getHeight();

	// TODO: Store all MSX lines in RawFrame and only scale the ones that fit
	//       on the PC screen, as a preparation for resizable output window.
	unsigned srcStartY = 0;
	while (srcStartY < srcHeight) {
		// get region with equal lineWidth
		unsigned lineWidth = paintFrame->getLineWidth(srcStartY);
		unsigned srcEndY = srcStartY + 1;
		while ((srcEndY < srcHeight) &&
		       (paintFrame->getLineWidth(srcEndY) == lineWidth)) {
			++srcEndY;
		}

		regions.emplace_back(srcStartY, srcEndY, lineWidth);

		// next region
		srcStartY = srcEndY;
	}
}

void PostProcessor::paint(OutputSurface& /*output*/)
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

	if ((screen.getViewOffset() != ivec2()) || // any part of the screen not covered by the viewport?
	    (deform == RenderSettings::DisplayDeform::_3D) || !paintFrame) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		if (!paintFrame) {
			return;
		}
	}

	auto dstSize = screen.getLogicalSize();
	bool needReUpload = false;

	// New scaler algorithm selected?
	if (auto algo = renderSettings.getScaleAlgorithm();
	    scaleAlgorithm != algo) {
		scaleAlgorithm = algo;
		currScaler = GLScalerFactory::createScaler(renderSettings, maxWidth, height * 2); // *2 for interlace   TODO only when canDoInterlace
		needReUpload = true;
	}

	if (needReUpload) {
		// Re-upload frame data, this is both
		//  - Chunks of RawFrame with a specific line width, possibly
		//    with some extra lines above and below each chunk that are
		//    also converted to this line width.
		//  - Extra data that is specific for the scaler (ATM only the
		//    hq and hqlite scalers require this).
		// Re-uploading the first is not strictly needed. But switching
		// scalers doesn't happen that often, so it also doesn't hurt
		// and it keeps the code simpler.
		uploadFrame();
	}

	auto scaleDstSize = currScaler->getOutputScaleSize(dstSize);

	auto createFBO = [](StoredFrame& frame, gl::ivec2 size) {
		if (frame.size == size) return;
		frame.size = size;

		frame.tex.bind();
		frame.tex.setInterpolation(true); // TODO
		glTexImage2D(GL_TEXTURE_2D,// target
 			0,                 // level
 			GL_RGB,            // internal format
			size.x,            // width
			size.y,            // height
 			0,                 // border
 			GL_RGB,            // format
 			GL_UNSIGNED_BYTE,  // type
 			nullptr);          // data
		frame.fbo = FrameBufferObject(frame.tex);
	};

	auto& renderedFrame = renderedFrames[frameCounter & 1];
	createFBO(renderedFrame, dstSize);

	auto prevFbo = FrameBufferObject::getCurrent();
	if (scaleDstSize != dstSize) {
		createFBO(area.frame, scaleDstSize);
		area.frame.fbo.activate();
	} else {
		// we can directly scale to the desired size
		renderedFrame.fbo.activate();
	}

	glViewport(0, 0, scaleDstSize.x, scaleDstSize.y);
	glBindTexture(GL_TEXTURE_2D, 0);

	auto srcHeight = int(paintFrame->getHeight());
 	for (const auto& r : regions) {
 		auto it = find_unguarded(textures, r.lineWidth, &TextureData::width);
 		auto* superImpose = superImposeVideoFrame
 		                  ? &superImposeTex : nullptr;
		ivec2 srcSize{int(r.lineWidth), srcHeight};
 		currScaler->scaleImage(
 			it->tex, superImpose,
			r.srcStartY, r.srcEndY, srcSize, scaleDstSize);
	}

	if (scaleDstSize != dstSize) {
		renderedFrame.fbo.activate();
		glViewport(0, 0, dstSize.x, dstSize.y);
		rescaleArea(scaleDstSize, dstSize);
 	}

	drawNoise();
	drawGlow(glow);

	FrameBufferObject::activate(prevFbo);
	renderedFrame.tex.bind();
	auto [x, y] = screen.getViewOffset();
	auto [w, h] = screen.getViewSize();
	glViewport(x, y, w, h);

	if (deform == RenderSettings::DisplayDeform::_3D) {
		drawMonitor3D();
	} else {
		float x1 = (320.0f - horStretch) * (1.0f / (2.0f * 320.0f));
		float x2 = 1.0f - x1;
		std::array tex = {
			vec2(x1, 1), vec2(x1, 0), vec2(x2, 0), vec2(x2, 1)
		};

		const auto& glContext = *gl::context;
		glContext.progTex.activate();
		glUniform4f(glContext.unifTexColor,
				1.0f, 1.0f, 1.0f, 1.0f);
		mat4 I;
		glUniformMatrix4fv(glContext.unifTexMvp, 1, GL_FALSE, I.data());

		glBindBuffer(GL_ARRAY_BUFFER, vbo.get());
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, stretchVBO.get());
		glBufferData(GL_ARRAY_BUFFER, sizeof(tex), tex.data(), GL_STREAM_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(1);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	storedFrame = true;
	//gl::checkGLError("PostProcessor::paint");
}

std::unique_ptr<RawFrame> PostProcessor::rotateFrames(
	std::unique_ptr<RawFrame> finishedFrame, EmuTime::param time)
{
	if (renderSettings.getInterleaveBlackFrame()) {
		auto delta = time - lastRotate; // time between last two calls
		auto middle = time + delta / 2; // estimate for middle between now
		                                // and next call
		setSyncPoint(middle);
	}
	lastRotate = time;

	// Figure out how many past frames we want to use.
	int numRequired = 1;
	bool doDeinterlace = false;
	bool doInterlace   = false;
	bool doDeflicker   = false;
	auto currType = finishedFrame->getField();
	if (canDoInterlace) {
		if (currType != FrameSource::FieldType::NONINTERLACED) {
			if (renderSettings.getDeinterlace()) {
				doDeinterlace = true;
				numRequired = 2;
			} else {
				doInterlace = true;
			}
		} else if (renderSettings.getDeflicker()) {
			doDeflicker = true;
			numRequired = 4;
		}
	}

	// Which frame can be returned (recycled) to caller. Prefer to return
	// the youngest frame to improve cache locality.
	int recycleIdx = (lastFramesCount < numRequired)
		? lastFramesCount++  // store one more
		: (numRequired - 1); // youngest that's no longer needed
	assert(recycleIdx < 4);
	auto recycleFrame = std::move(lastFrames[recycleIdx]); // might be nullptr

	// Insert new frame in front of lastFrames[], shift older frames
	std::move_backward(&lastFrames[0], &lastFrames[recycleIdx],
	                   &lastFrames[recycleIdx + 1]);
	lastFrames[0] = std::move(finishedFrame);

	// Are enough frames available?
	if (lastFramesCount >= numRequired) {
		// Only the last 'numRequired' are kept up to date.
		lastFramesCount = numRequired;
	} else {
		// Not enough past frames, fall back to 'regular' rendering.
		// This situation can only occur when:
		// - The very first frame we render needs to be deinterlaced.
		//   In other case we have at least one valid frame from the
		//   past plus one new frame passed via the 'finishedFrame'
		//   parameter.
		// - Or when (re)enabling the deflicker setting. Typically only
		//   1 frame in lastFrames[] is kept up-to-date (and we're
		//   given 1 new frame), so it can take up-to 2 frame after
		//   enabling deflicker before it actually takes effect.
		doDeinterlace = false;
		doInterlace   = false;
		doDeflicker   = false;
	}

	// Setup the to-be-painted frame
	if (doDeinterlace) {
		if (currType == FrameSource::FieldType::ODD) {
			deinterlacedFrame->init(lastFrames[1].get(), lastFrames[0].get());
		} else {
			deinterlacedFrame->init(lastFrames[0].get(), lastFrames[1].get());
		}
		paintFrame = deinterlacedFrame.get();
	} else if (doInterlace) {
		interlacedFrame->init(
			lastFrames[0].get(),
			(currType == FrameSource::FieldType::ODD) ? 1 : 0);
		paintFrame = interlacedFrame.get();
	} else if (doDeflicker) {
		deflicker->init();
		paintFrame = deflicker.get();
	} else {
		paintFrame = lastFrames[0].get();
	}
	if (superImposeVdpFrame) {
		superImposedFrame->init(paintFrame, superImposeVdpFrame);
		paintFrame = superImposedFrame.get();
	}

	// Possibly record this frame
	if (recorder && needRecord()) {
		try {
			recorder->addImage(paintFrame, time);
		} catch (MSXException& e) {
			getCliComm().printWarning(
				"Recording stopped with error: ",
				e.getMessage());
			recorder->stop();
			assert(!recorder);
		}
	}

	// Return recycled frame to the caller
	std::unique_ptr<RawFrame> reuseFrame = [&] {
		if (canDoInterlace) {
			if (!recycleFrame) [[unlikely]] {
				recycleFrame = std::make_unique<RawFrame>(maxWidth, height);
			}
			return std::move(recycleFrame);
		} else {
			return std::move(lastFrames[0]);
		}
	}();

	uploadFrame();
	++frameCounter;
	noiseX = random_float(0.0f, 1.0f);
	noiseY = random_float(0.0f, 1.0f);
	return reuseFrame;
}

void PostProcessor::update(const Setting& setting) noexcept
{
	VideoLayer::update(setting);
	const auto& noiseSetting = renderSettings.getNoiseSetting();
	const auto& horizontalStretch = renderSettings.getHorizontalStretchSetting();
	if (&setting == &noiseSetting) {
		preCalcNoise(noiseSetting.getFloat());
	} else if (&setting == &horizontalStretch) {
		preCalcMonitor3D(horizontalStretch.getFloat());
	}
}

void PostProcessor::uploadFrame()
{
	createRegions();

	const unsigned srcHeight = paintFrame->getHeight();
	for (const auto& r : regions) {
		// upload data
		// TODO get before/after data from scaler
		int before = 1;
		unsigned after  = 1;
		uploadBlock(narrow<unsigned>(std::max(0, narrow<int>(r.srcStartY) - before)),
		            std::min(srcHeight, r.srcEndY + after),
		            r.lineWidth);
	}

	if (superImposeVideoFrame) {
		int w = narrow<GLsizei>(superImposeVideoFrame->getWidth());
		int h = narrow<GLsizei>(superImposeVideoFrame->getHeight());
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
			superImposeVideoFrame->getLineDirect(0).data()); // data
	}
}

void PostProcessor::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth)
{
	// create texture on demand
	auto it = std::ranges::find(textures, lineWidth, &TextureData::width);
	if (it == end(textures)) {
		TextureData textureData;
		textureData.tex.resize(narrow<GLsizei>(lineWidth),
		                       narrow<GLsizei>(height * 2)); // *2 for interlace   TODO only when canDoInterlace
		textures.push_back(std::move(textureData));
		it = end(textures) - 1;
	}
	auto& tex = it->tex;

	// bind texture
	tex.bind();

	// upload data
	pbo.bind();
	auto mapped = pbo.mapWrite();
	auto numLines = srcEndY - srcStartY;
	for (auto yy : xrange(numLines)) {
		auto dest = mapped.subspan(yy * size_t(lineWidth), lineWidth);
		auto line = paintFrame->getLine(narrow<int>(yy + srcStartY), dest);
		if (line.data() != dest.data()) {
			copy_to_range(line, dest);
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
		GL_TEXTURE_2D,            // target
		0,                        // level
		0,                        // offset x
		narrow<GLint>(srcStartY), // offset y
		narrow<GLint>(lineWidth), // width
		narrow<GLint>(numLines),  // height
		GL_RGBA,                  // format
		GL_UNSIGNED_BYTE,         // type
		mapped.data());           // data
	pbo.unbind();

	// possibly upload scaler specific data
	if (currScaler) {
		currScaler->uploadBlock(srcStartY, srcEndY, lineWidth, *paintFrame);
	}
}

void PostProcessor::initAreaScaler()
{
	auto& program = area.program;
	VertexShader   vertexShader  ("rescale.vert");
	FragmentShader fragmentShader("rescale.frag");
	program.attach(vertexShader);
	program.attach(fragmentShader);
	program.bindAttribLocation(0, "a_position");
	program.bindAttribLocation(1, "a_texCoord");
	program.link();
	program.activate();
	glUniform1i(program.getUniformLocation("tex"), 0);
	area.unifMvpMatrix     = program.getUniformLocation("u_mvpMatrix");
	area.unifTexelCount    = program.getUniformLocation("texelCount");
	area.unifPixelCount    = program.getUniformLocation("pixelCount");
	area.unifTexelSize     = program.getUniformLocation("texelSize");
	area.unifPixelSize     = program.getUniformLocation("pixelSize");
	area.unifHalfTexelSize = program.getUniformLocation("halfTexelSize");
	area.unifHalfPixelSize = program.getUniformLocation("halfPixelSize");
}

void PostProcessor::rescaleArea(gl::ivec2 srcSize, gl::ivec2 dstSize)
{
	area.program.activate();

	area.frame.tex.bind();
	area.frame.tex.setInterpolation(false);

	auto M = ortho(dstSize.x, dstSize.y);
	glUniformMatrix4fv(area.unifMvpMatrix, 1, GL_FALSE, M.data());
	gl::vec2 texelCount(srcSize);
	glUniform2f(area.unifTexelCount, texelCount.x, texelCount.y);
	gl::vec2 pixelCount(dstSize);
	glUniform2f(area.unifPixelCount, pixelCount.x, pixelCount.y);
	gl::vec2 texelSize = 1.0f / texelCount;
	glUniform2f(area.unifTexelSize, texelSize.x, texelSize.y);
	gl::vec2 pixelSize = 1.0f / pixelCount;
	glUniform2f(area.unifPixelSize, pixelSize.x, pixelSize.y);
	gl::vec2 halfTexelSize = 0.5f * texelSize;
	glUniform2f(area.unifHalfTexelSize, halfTexelSize.x, halfTexelSize.y);
	gl::vec2 halfPixelSize = 0.5f * pixelSize;
	glUniform2f(area.unifHalfPixelSize, halfPixelSize.x, halfPixelSize.y);

	std::array pos = {
		vec2(        0.0f, pixelCount.y),
		vec2(pixelCount.x, pixelCount.y),
		vec2(pixelCount.x, 0.0f),
		vec2(        0.0f, 0.0f),
	};
	glBindBuffer(GL_ARRAY_BUFFER, area.posBuffer.get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos.data(), GL_STREAM_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	std::array tex = { // TODO in the future integrate horizontal stretch
		vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1),
	};
	glBindBuffer(GL_ARRAY_BUFFER, area.texBuffer.get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(tex), tex.data(), GL_STREAM_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void PostProcessor::drawGlow(int glow)
{
	if ((glow == 0) || !storedFrame) return;

	const auto& glContext = *gl::context;
	glContext.progTex.activate();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	renderedFrames[(frameCounter & 1) ^ 1].tex.bind();
	glUniform4f(glContext.unifTexColor,
	            1.0f, 1.0f, 1.0f, narrow<float>(glow) * (31.0f / 3200.0f));
	mat4 I;
	glUniformMatrix4fv(glContext.unifTexMvp, 1, GL_FALSE, I.data());

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

void PostProcessor::preCalcNoise(float factor)
{
	std::array<uint8_t, 256 * 256> buf1;
	std::array<uint8_t, 256 * 256> buf2;
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::normal_distribution<float> distribution(0.0f, 1.0f);
	for (auto i : xrange(256 * 256)) {
		float r = distribution(generator);
		int s = std::clamp(int(roundf(r * factor)), -255, 255);
		buf1[i] = narrow<uint8_t>((s > 0) ?  s : 0);
		buf2[i] = narrow<uint8_t>((s < 0) ? -s : 0);
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
		buf1.data());     // data
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
		buf2.data());     // data
#if OPENGL_VERSION >= OPENGL_3_3
	GLint swizzleMask2[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask2);
#endif
}

void PostProcessor::drawNoise() const
{
	if (renderSettings.getNoise() == 0.0f) return;

	// Rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise.
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
	vec2 noise(noiseX, noiseY);
	const std::array tex = {
		noise + vec2(0.0f, 1.875f),
		noise + vec2(2.0f, 1.875f),
		noise + vec2(2.0f, 0.0f  ),
		noise + vec2(0.0f, 0.0f  ),
	};

	const auto& glContext = *gl::context;
	glContext.progTex.activate();

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glUniform4f(glContext.unifTexColor, 1.0f, 1.0f, 1.0f, 1.0f);
	mat4 I;
	glUniformMatrix4fv(glContext.unifTexMvp, 1, GL_FALSE, I.data());

	unsigned seq = frameCounter & 7;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos[seq].data());
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex.data());
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

static constexpr int GRID_SIZE = 16;
static constexpr int GRID_SIZE1 = GRID_SIZE + 1;
static constexpr int NUM_INDICES = (GRID_SIZE1 * 2 + 2) * GRID_SIZE - 2;
struct Vertex {
	vec3 position;
	vec3 normal;
	vec2 tex;
};

void PostProcessor::preCalcMonitor3D(float width)
{
	// precalculate vertex-positions, -normals and -texture-coordinates
	std::array<std::array<Vertex, GRID_SIZE1>, GRID_SIZE1> vertices;

	constexpr float GRID_SIZE2 = float(GRID_SIZE) * 0.5f;
	float s = width * (1.0f / 320.0f);
	float b = (320.0f - width) * (1.0f / (2.0f * 320.0f));

	for (auto sx : xrange(GRID_SIZE1)) {
		for (auto sy : xrange(GRID_SIZE1)) {
			Vertex& v = vertices[sx][sy];
			float x = (narrow<float>(sx) - GRID_SIZE2) / GRID_SIZE2;
			float y = (narrow<float>(sy) - GRID_SIZE2) / GRID_SIZE2;

			v.position = vec3(x, y, (x * x + y * y) * (1.0f / -12.0f));
			v.normal = normalize(vec3(x * (1.0f / 6.0f), y * (1.0f / 6.0f), 1.0f)) * 1.2f;
			v.tex = vec2((float(sx) / GRID_SIZE) * s + b,
			              float(sy) / GRID_SIZE);
		}
	}

	// calculate indices
	std::array<uint16_t, NUM_INDICES> indices;

	uint16_t* ind = indices.data();
	for (auto y : xrange(GRID_SIZE)) {
		for (auto x : xrange(GRID_SIZE1)) {
			*ind++ = narrow<uint16_t>((y + 0) * GRID_SIZE1 + x);
			*ind++ = narrow<uint16_t>((y + 1) * GRID_SIZE1 + x);
		}
		// skip 2, filled in later
		ind += 2;
	}
	assert((ind - indices.data()) == NUM_INDICES + 2);
	ind = indices.data();
	repeat(GRID_SIZE - 1, [&] {
		ind += 2 * GRID_SIZE1;
		// repeat prev and next index to restart strip
		ind[0] = ind[-1];
		ind[1] = ind[ 2];
		ind += 2;
	});

	// upload calculated values to buffers
	glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer.get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(),
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer.get());
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(),
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// calculate transformation matrices
	mat4 proj = frustum(-1, 1, -1, 1, 1, 10);
	mat4 tran = translate(vec3(0.0f, 0.4f, -2.0f));
	mat4 rotX = rotateX(radians(-10.0f));
	mat4 scal = scale(vec3(2.2f, 2.2f, 2.2f));

	mat3 normal(rotX);
	mat4 mvp = proj * tran * rotX * scal;

	// set uniforms
	monitor3DProg.activate();
	glUniform1i(monitor3DProg.getUniformLocation("u_tex"), 0);
	glUniformMatrix4fv(monitor3DProg.getUniformLocation("u_mvpMatrix"),
		1, GL_FALSE, mvp.data());
	glUniformMatrix3fv(monitor3DProg.getUniformLocation("u_normalMatrix"),
		1, GL_FALSE, normal.data());
}

void PostProcessor::drawMonitor3D() const
{
	monitor3DProg.activate();

	char* base = nullptr;
	glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer.get());
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer.get());
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
