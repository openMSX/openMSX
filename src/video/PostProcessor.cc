// $Id$

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "OutputSurface.hh"
#include "DeinterlacedFrame.hh"
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
	currScalerID = (ScalerID)-1; // not a valid scaler

	currFrame = new RawFrame(screen.getFormat(), sizeof(Pixel),
	                         maxWidth, height);
	prevFrame = new RawFrame(screen.getFormat(), sizeof(Pixel),
	                         maxWidth, height);
	deinterlacedFrame = new DeinterlacedFrame(screen.getFormat());
}

template <class Pixel>
PostProcessor<Pixel>::~PostProcessor()
{
	delete currFrame;
	delete prevFrame;
	delete deinterlacedFrame;
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
	const FrameSource::FieldType field = currFrame->getField();
	const bool deinterlace = field != FrameSource::FIELD_NONINTERLACED
		&& renderSettings.getDeinterlace().getValue();
		
	// TODO: When frames are being skipped or if (de)interlace was just
	//       turned on, it's not guaranteed that prevFrame is a
	//       different field from currFrame.
	//       Or in the case of frame skip, it might be the right field,
	//       but from several frames ago.
	FrameSource* frame;
	if (!deinterlace) {
		frame = currFrame;
	} else {
		deinterlacedFrame->init(
		    (field == FrameSource::FIELD_ODD) ? prevFrame : currFrame,
		    (field == FrameSource::FIELD_ODD) ? currFrame : prevFrame);
		frame = deinterlacedFrame;
	}

	// New scaler algorithm selected?
	ScalerID scalerID = renderSettings.getScaler().getValue();
	if (currScalerID != scalerID) {
		currScaler = Scaler<Pixel>::createScaler(
			scalerID, screen.getFormat(), renderSettings);
		currScalerID = scalerID;
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
	// TODO this is only correct for scalefactor=2 
	unsigned dstStartY = ((field == FrameSource::FIELD_ODD) && !deinterlace)
	                   ? 1 : 0;
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
		if (lineWidth == 1) {
			paintBlank(*frame, srcStep, dstStep, srcStartY, srcEndY,
			           screen, dstStartY, dstHeight);
		} else {
			dstEndY = std::min(dstEndY, dstHeight);
			currScaler->scaleImage(
				*frame, srcStartY, srcEndY, lineWidth, // source
				screen, dstStartY, dstEndY); // dest
		}

		// next region
		srcStartY = srcEndY;
		dstStartY = dstEndY;
	}
	screen.drawFrameBuffer();
}


template <typename Pixel>
static unsigned allSameColor(FrameSource& frame, unsigned y, unsigned step,
                             Pixel color)
{
	for (unsigned i = 0; i < step; ++i) {
		if (*frame.getLinePtr(y + i, (Pixel*)0) != color) {
			return false;
		}
	}
	return true;
}

template <typename Pixel>
void PostProcessor<Pixel>::paintBlank(
		FrameSource& frame, unsigned srcStep, unsigned dstStep,
		unsigned srcStartY, unsigned srcEndY,
		OutputSurface& screen, unsigned dstStartY, unsigned dstHeight)
{
	while (srcStartY < srcEndY) {
		Pixel color = *frame.getLinePtr(srcStartY, (Pixel*)0);
		bool sameColor = allSameColor(frame, srcStartY + 1,
		                              srcStep - 1, color);
		unsigned srcY = srcStartY + srcStep;
		unsigned dstY = dstStartY + dstStep;
		if (sameColor) {
			// units with all lines the same color
			while ((srcY < srcEndY) &&
			       allSameColor(frame, srcY, srcStep, color)) {
				srcY += srcStep;
				dstY += dstStep;
			}
			dstY = std::min(dstY, dstHeight);
			currScaler->scaleBlank(color, screen, dstStartY, dstY);
		} else {
			// units with lines in different colors
			while (srcY < srcEndY) {
				Pixel color2 = *frame.getLinePtr(srcY, (Pixel*)0);
				if (allSameColor(frame, srcStartY + 1,
				                 srcStep - 1, color2)) {
					break;
				}
				srcY += srcStep;
				dstY += dstStep;
			}
			dstY = std::min(dstY, dstHeight);
			// 320 because that is most likely best optimized
			currScaler->scaleImage(frame, srcStartY, srcY, 320,
			                       screen, dstStartY, dstY);
		}
		srcStartY = srcY;
		dstStartY = dstY;
	}
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
