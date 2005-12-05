// $Id$

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "Scaler.hh"
#include "ScalerFactory.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "OutputSurface.hh"
#include "DeinterlacedFrame.hh"
#include "DoubledFrame.hh"
#include "RawFrame.hh"
#include <cassert>

namespace openmsx {

template <class Pixel>
PostProcessor<Pixel>::PostProcessor(CommandController& commandController,
	RenderSettings& renderSettings_, Display& display,
	OutputSurface& screen_, VideoSource videoSource,
	unsigned maxWidth, unsigned height)
	: VideoLayer(videoSource, commandController, renderSettings_, display)
	, renderSettings(renderSettings_)
	, screen(screen_)
{
	scaleAlgorithm = (RenderSettings::ScaleAlgorithm)-1; // not a valid scaler
	scaleFactor = (unsigned)-1;

	currFrame = new RawFrame(screen.getFormat(), sizeof(Pixel),
	                         maxWidth, height);
	prevFrame = new RawFrame(screen.getFormat(), sizeof(Pixel),
	                         maxWidth, height);
	deinterlacedFrame = new DeinterlacedFrame(screen.getFormat());
	interlacedFrame   = new DoubledFrame     (screen.getFormat());
}

template <class Pixel>
PostProcessor<Pixel>::~PostProcessor()
{
	delete currFrame;
	delete prevFrame;
	delete deinterlacedFrame;
	delete interlacedFrame;
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

static unsigned getLineWidth(FrameSource* frame, unsigned y, unsigned step)
{
	unsigned result = frame->getLineWidth(y);
	for (unsigned i = 1; i < step; ++i) {
		result = std::max(result, frame->getLineWidth(y + i));
	}
	return result;
}

template <class Pixel>
void PostProcessor<Pixel>::paint()
{
	// TODO: When frames are being skipped or if (de)interlace was just
	//       turned on, it's not guaranteed that prevFrame is a
	//       different field from currFrame.
	//       Or in the case of frame skip, it might be the right field,
	//       but from several frames ago.
	FrameSource* frame;
	FrameSource::FieldType field = currFrame->getField();
	if (field != FrameSource::FIELD_NONINTERLACED) {
		if (renderSettings.getDeinterlace().getValue()) {
			// deinterlaced
			if (field == FrameSource::FIELD_ODD) {
				deinterlacedFrame->init(prevFrame, currFrame);
			} else {
				deinterlacedFrame->init(currFrame, prevFrame);
			}
			frame = deinterlacedFrame;
		} else {
			// interlaced
			interlacedFrame->init(currFrame,
				(field == FrameSource::FIELD_ODD) ? 1 : 0);
			frame = interlacedFrame;
		}
	} else {
		// non interlaced
		frame = currFrame;
	}

	// New scaler algorithm selected?
	RenderSettings::ScaleAlgorithm algo =
		renderSettings.getScaleAlgorithm().getValue();
	unsigned factor = renderSettings.getScaleFactor().getValue();
	if ((scaleAlgorithm != algo) || (scaleFactor != factor)) {
		scaleAlgorithm = algo;
		scaleFactor = factor;
		currScaler = ScalerFactory<Pixel>::createScaler(
			screen.getFormat(), renderSettings);
	}

	// Scale image.
	const unsigned srcHeight = frame->getHeight();
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
		unsigned lineWidth = getLineWidth(frame, srcStartY, srcStep);
		unsigned srcEndY = srcStartY + srcStep;
		unsigned dstEndY = dstStartY + dstStep;
		while ((srcEndY < srcHeight) && (dstEndY < dstHeight) &&
		       (getLineWidth(frame, srcEndY, srcStep) == lineWidth)) {
			srcEndY += srcStep;
			dstEndY += dstStep;
		}

		// fill region
		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	srcStartY, srcEndY, lineWidth );
		currScaler->scaleImage(
			*frame, srcStartY, srcEndY, lineWidth, // source
			screen, dstStartY, dstEndY); // dest

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}
	screen.drawFrameBuffer();
}

template <class Pixel>
const std::string& PostProcessor<Pixel>::getName()
{
	static const std::string V99x8_NAME = "V99x8 PostProcessor";
	static const std::string V9990_NAME = "V9990 PostProcessor";
	return (getVideoSource() == VIDEO_GFX9000) ? V9990_NAME : V99x8_NAME;
}

template <class Pixel>
RawFrame* PostProcessor<Pixel>::rotateFrames(
	RawFrame* finishedFrame, FrameSource::FieldType field)
{
	RawFrame* reuseFrame = prevFrame;
	prevFrame = currFrame;
	currFrame = finishedFrame;
	reuseFrame->init(field);
	return reuseFrame;
}


// Force template instantiation.
template class PostProcessor<word>;
template class PostProcessor<unsigned>;

} // namespace openmsx
