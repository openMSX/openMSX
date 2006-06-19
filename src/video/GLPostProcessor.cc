// $Id$

#include "GLPostProcessor.hh"
#include "GLScaler.hh"
#include "GLScalerFactory.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "VisibleSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "RawFrame.hh"
#include "Math.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

GLPostProcessor::GLPostProcessor(
	CommandController& commandController, Display& display,
	VisibleSurface& screen, VideoSource videoSource,
	unsigned maxWidth, unsigned height_)
	: PostProcessor(commandController, display, screen, videoSource,
	                maxWidth, height_)
	, height(height_)
{
	paintFrame = NULL;

	scaleAlgorithm = (RenderSettings::ScaleAlgorithm)-1; // not a valid scaler

	noiseTextureA.setImage(256, 256);
	noiseTextureB.setImage(256, 256);
	noiseSeq = 0;
	noiseX = 0.0;
	noiseY = 0.0;
	preCalcNoise(renderSettings.getNoise().getValue());

	renderSettings.getNoise().attach(*this);
}

GLPostProcessor::~GLPostProcessor()
{
	renderSettings.getNoise().detach(*this);

	for (Textures::iterator it = textures.begin();
	     it != textures.end(); ++it) {
		delete it->second;
	}
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

		regions.push_back(Region(srcStartY, srcEndY,
		                         dstStartY, dstEndY,
		                         lineWidth));

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}
}


void GLPostProcessor::paint()
{
	if (!paintFrame) {
		// TODO: Paint blackness.
		return;
	}

	// New scaler algorithm selected?
	RenderSettings::ScaleAlgorithm algo =
		renderSettings.getScaleAlgorithm().getValue();
	if (scaleAlgorithm != algo) {
		scaleAlgorithm = algo;
		currScaler = GLScalerFactory::createScaler(renderSettings);
	}

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (Regions::const_iterator it = regions.begin();
	     it != regions.end(); ++it) {
		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	it->srcStartY, it->srcEndY, it->lineWidth);
		assert(textures[it->lineWidth]);
		currScaler->scaleImage(
			*textures[it->lineWidth],
			it->srcStartY, it->srcEndY, it->lineWidth,      // src
			it->dstStartY, it->dstEndY, screen.getWidth()); // dst
		//GLUtil::checkGLError("GLPostProcessor::paint");
	}

	ShaderProgram::deactivate();

	drawNoise();
}

RawFrame* GLPostProcessor::rotateFrames(
	RawFrame* finishedFrame, FrameSource::FieldType field)
{
	RawFrame* reuseFrame = PostProcessor::rotateFrames(finishedFrame, field);
	uploadFrame();
	noiseSeq = (noiseSeq + 1) & 7;
	noiseX = (double)rand() / RAND_MAX;
	noiseY = (double)rand() / RAND_MAX;
	return reuseFrame;
}

void GLPostProcessor::update(const Setting& setting)
{
	VideoLayer::update(setting);
	FloatSetting& noiseSetting = renderSettings.getNoise();
	if (&setting == &noiseSetting) {
		preCalcNoise(noiseSetting.getValue());
	}
}

void GLPostProcessor::uploadFrame()
{
	// TODO: When frames are being skipped or if (de)interlace was just
	//       turned on, it's not guaranteed that prevFrame is a
	//       different field from currFrame.
	//       Or in the case of frame skip, it might be the right field,
	//       but from several frames ago.
	FrameSource::FieldType field = currFrame->getField();
	if (field != FrameSource::FIELD_NONINTERLACED) {
		if (renderSettings.getDeinterlace().getValue()) {
			// deinterlaced
			if (field == FrameSource::FIELD_ODD) {
				deinterlacedFrame->init(prevFrame, currFrame);
			} else {
				deinterlacedFrame->init(currFrame, prevFrame);
			}
			paintFrame = deinterlacedFrame;
		} else {
			// interlaced
			interlacedFrame->init(currFrame,
				(field == FrameSource::FIELD_ODD) ? 1 : 0);
			paintFrame = interlacedFrame;
		}
	} else {
		// non interlaced
		paintFrame = currFrame;
	}
	createRegions();

	const unsigned srcHeight = paintFrame->getHeight();
	for (Regions::const_iterator it = regions.begin();
	     it != regions.end(); ++it) {
		// bind texture (create if needed)
		TextureRectangle* tex;
		Textures::iterator it2 = textures.find(it->lineWidth);
		if (it2 != textures.end()) {
			tex = it2->second;
		} else {
			tex = new TextureRectangle();
			tex->setImage(it->lineWidth, height * 2);
			textures[it->lineWidth] = tex;
		}
		tex->bind();
		// upload data
		// TODO get before/after data from scaler
		unsigned before = 1;
		unsigned after  = 1;
		uploadBlock(std::max<int>(0,         it->srcStartY - before),
		            std::min<int>(srcHeight, it->srcEndY   + after),
		            it->lineWidth);
	}
}

// require: correct texture is already bound
void GLPostProcessor::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth)
{
	for (unsigned y = srcStartY; y < srcEndY; ++y) {
		unsigned* dummy = 0;
		glTexSubImage2D(
			GL_TEXTURE_RECTANGLE_ARB, // target
			0,                        // level
			0,                        // offset x
			y,                        // offset y
			lineWidth,                // width
			1,                        // height
			GL_RGBA,                  // format
			GL_UNSIGNED_BYTE,         // type
			paintFrame->getLinePtr(y, lineWidth, dummy)); // data
		//GLUtil::checkGLError("GLPostProcessor::uploadFrame");
		paintFrame->freeLineBuffers(); // ASAP to keep cache warm
	}
}

void GLPostProcessor::preCalcNoise(double factor)
{
	GLbyte buf1[256 * 256];
	GLbyte buf2[256 * 256];
	for (int i = 0; i < 256 * 256; i += 2) {
		double r1, r2;
		Math::gaussian2(r1, r2);
		int s1 = Math::clip<-255, 255>(r1, factor);
		buf1[i + 0] = (s1 > 0) ?  s1 : 0;
		buf2[i + 0] = (s1 < 0) ? -s1 : 0;
		int s2 = Math::clip<-255, 255>(r2, factor);
		buf1[i + 1] = (s2 > 0) ?  s2 : 0;
		buf2[i + 1] = (s2 < 0) ? -s2 : 0;
	}
	noiseTextureA.updateImage(0, 0, 256, 256, buf1);
	noiseTextureB.updateImage(0, 0, 256, 256, buf2);
}

void GLPostProcessor::drawNoise()
{
	if (renderSettings.getNoise().getValue() == 0) return;

	// Rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise.
	static const int coord[8][4][2] = {
		{ {   0,   0 }, { 320,   0 }, { 320, 240 }, {   0, 240 } },
		{ {   0, 240 }, { 320, 240 }, { 320,   0 }, {   0,   0 } },
		{ {   0, 240 }, {   0,   0 }, { 320,   0 }, { 320, 240 } },
		{ { 320, 240 }, { 320,   0 }, {   0,   0 }, {   0, 240 } },
		{ { 320, 240 }, {   0, 240 }, {   0,   0 }, { 320,   0 } },
		{ { 320,   0 }, {   0,   0 }, {   0, 240 }, { 320, 240 } },
		{ { 320,   0 }, { 320, 240 }, {   0, 240 }, {   0,   0 } },
		{ {   0,   0 }, {   0, 240 }, { 320, 240 }, { 320,   0 } }
	};
	int zoom = renderSettings.getScaleFactor().getValue();

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	noiseTextureA.bind();
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + noiseX, 1.875f + noiseY);
	glVertex2i(coord[noiseSeq][0][0] * zoom, coord[noiseSeq][0][1] * zoom);
	glTexCoord2f(2.5f + noiseX, 1.875f + noiseY);
	glVertex2i(coord[noiseSeq][1][0] * zoom, coord[noiseSeq][1][1] * zoom);
	glTexCoord2f(2.5f + noiseX, 0.000f + noiseY);
	glVertex2i(coord[noiseSeq][2][0] * zoom, coord[noiseSeq][2][1] * zoom);
	glTexCoord2f(0.0f + noiseX, 0.000f + noiseY);
	glVertex2i(coord[noiseSeq][3][0] * zoom, coord[noiseSeq][3][1] * zoom);
	glEnd();
	// Note: If glBlendEquation is not present, the second noise texture will
	//       be added instead of subtracted, which means there will be no noise
	//       on white pixels. A pity, but it's better than no noise at all.
	if (glBlendEquation) glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	noiseTextureB.bind();
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + noiseX, 1.875f + noiseY);
	glVertex2i(coord[noiseSeq][0][0] * zoom, coord[noiseSeq][0][1] * zoom);
	glTexCoord2f(2.5f + noiseX, 1.875f + noiseY);
	glVertex2i(coord[noiseSeq][1][0] * zoom, coord[noiseSeq][1][1] * zoom);
	glTexCoord2f(2.5f + noiseX, 0.000f + noiseY);
	glVertex2i(coord[noiseSeq][2][0] * zoom, coord[noiseSeq][2][1] * zoom);
	glTexCoord2f(0.0f + noiseX, 0.000f + noiseY);
	glVertex2i(coord[noiseSeq][3][0] * zoom, coord[noiseSeq][3][1] * zoom);
	glEnd();
	glPopAttrib();
	if (glBlendEquation) glBlendEquation(GL_FUNC_ADD);
}

} // namespace openmsx
