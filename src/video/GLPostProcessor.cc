// $Id$

#include "GLPostProcessor.hh"
#include "GLScaler.hh"
#include "GLScalerFactory.hh"
#include "BooleanSetting.hh"
#include "VisibleSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "RawFrame.hh"
#include <cassert>

namespace openmsx {

GLPostProcessor::GLPostProcessor(
	CommandController& commandController, Display& display,
	VisibleSurface& screen_, VideoSource videoSource,
	unsigned maxWidth, unsigned height
	)
	: PostProcessor(
		commandController, display, screen_, videoSource, maxWidth, height
		)
{
	paintFrame = NULL;
	paintTexture.setImage(maxWidth, height * 2);

	scaleAlgorithm = (RenderSettings::ScaleAlgorithm)-1; // not a valid scaler
}

GLPostProcessor::~GLPostProcessor()
{
}

/** Calculate greatest common divider of two strictly positive integers.
  * Classical implementation is like this:
  *    while (unsigned t = b % a) { b = a; a = t; }
  *    return a;
  * The following implementation avoids the costly modulo operation. It
  * is about 40% faster on my machine.
  *
  * require: a != 0  &&  b != 0
  */
static inline unsigned gcd(unsigned a, unsigned b)
{
	unsigned k = 0;
	while (((a & 1) == 0) && ((b & 1) == 0)) {
		a >>= 1; b >>= 1; ++k;
	}

	// either a or b (or both) is odd
	while ((a & 1) == 0) a >>= 1;
	while ((b & 1) == 0) b >>= 1;

	// both a and b odd
	while (a != b) {
		if (a >= b) {
			a -= b;
			do { a >>= 1; } while ((a & 1) == 0);
		} else {
			b -= a;
			do { b >>= 1; } while ((b & 1) == 0);
		}
	}
	return b << k;
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

	// Scale image.
	const unsigned srcHeight = paintFrame->getHeight();
	const unsigned dstHeight = screen.getHeight();

	unsigned g = gcd(srcHeight, dstHeight);
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

		// fill region
		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	srcStartY, srcEndY, lineWidth );
		currScaler->scaleImage(
			paintTexture,
			srcStartY, srcEndY, lineWidth, // source
			dstStartY, dstEndY, screen.getWidth() // dest
			);

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}

	ShaderProgram::deactivate();
}

RawFrame* GLPostProcessor::rotateFrames(
	RawFrame* finishedFrame, FrameSource::FieldType field)
{
	RawFrame* reuseFrame = PostProcessor::rotateFrames(finishedFrame, field);
	uploadFrame();
	return reuseFrame;
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

	const unsigned srcHeight = paintFrame->getHeight();
	paintTexture.bind();
	for (unsigned y = 0; y < srcHeight; y++) {
		// Upload line to texture.
		// TODO: Is not having to rebind the same texture worth the effort?
		unsigned lineWidth = paintFrame->getLineWidth(y);
		glTexSubImage2D(
			GL_TEXTURE_2D,    // target
			0,                // level
			0,                // offset x
			y,                // offset y
			lineWidth,        // width
			1,                // height
			GL_RGBA,          // format
			GL_UNSIGNED_BYTE, // type
			paintFrame->getLinePtr(y, (unsigned*)0) // data
			);
	}
}

} // namespace openmsx
