// $Id$

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "Scaler.hh"
#include "BooleanSetting.hh"
#include "MemoryOps.hh"
#include <SDL.h>
#include <cassert>


namespace openmsx {

template <class Pixel>
PostProcessor<Pixel>::PostProcessor(CommandController& commandController,
	RenderSettings& renderSettings_, Display& display,
	SDL_Surface* screen, VideoSource videoSource,
	unsigned maxWidth, unsigned height)
	: VideoLayer(videoSource, commandController, renderSettings_, display)
	, renderSettings(renderSettings_)
{
	this->screen = screen;
	currScalerID = (ScalerID)-1; // not a valid scaler

	currFrame = new RawFrame(sizeof(Pixel), maxWidth, height);
	prevFrame = new RawFrame(sizeof(Pixel), maxWidth, height);
}

template <class Pixel>
PostProcessor<Pixel>::~PostProcessor()
{
	delete currFrame;
	delete prevFrame;
}

template <class Pixel>
void PostProcessor<Pixel>::paint()
{
	const RawFrame::FieldType field = currFrame->getField();
	const bool deinterlace = (field != RawFrame::FIELD_NONINTERLACED) &&
                        renderSettings.getDeinterlace().getValue();
	RawFrame* frameEven = 0;
	RawFrame* frameOdd  = 0;
	if (deinterlace) {
		if (field == RawFrame::FIELD_ODD) {
			frameEven = prevFrame;
			frameOdd  = currFrame;
		} else {
			frameEven = currFrame;
			frameOdd  = prevFrame;
		}
	}

	// New scaler algorithm selected?
	ScalerID scalerID = renderSettings.getScaler().getValue();
	if (currScalerID != scalerID) {
		currScaler = Scaler<Pixel>::createScaler(
			scalerID, screen->format, renderSettings);
		currScalerID = scalerID;
	}

	// Scale image.
	// TODO: Check deinterlace first: now that we keep LineContent info per
	//       frame, we should take the lineWidth of two frames into account.
	const unsigned srcHeight = 240;
	const unsigned dstHeight = screen->h;
	const bool lower = field == RawFrame::FIELD_ODD;
	const unsigned lineZoom = dstHeight / srcHeight;
	// TODO: Store all MSX lines in RawFrame and only scale the ones that fit
	//       on the PC screen, as a preparation for resizable output window.
	unsigned srcStartY = 0;
	unsigned dstStartY = lower && !deinterlace ? 1 : 0;
	while (dstStartY < dstHeight) {
		// Currently this is true because the source frame height
		// is always >= dstHeight/lineZoom.
		assert(srcStartY < srcHeight);

		unsigned lineWidth = currFrame->getLineWidth(srcStartY);
		unsigned srcEndY = srcStartY + 1;
		unsigned dstEndY = dstStartY + lineZoom;
		while ((srcEndY < srcHeight) && (dstEndY < dstHeight) &&
		       (currFrame->getLineWidth(srcEndY) == lineWidth)) {
			srcEndY++;
			dstEndY += lineZoom;
		}
		if (dstEndY > dstHeight) dstEndY = dstHeight;

		if (lineWidth == 0) {
			// Reduce area to same-colour starting segment.
			const Pixel colour = *currFrame->getLinePtr(srcStartY, (Pixel*)0);
			for (unsigned y = srcStartY + 1; y < srcEndY; y++) {
				const Pixel colour2 = *currFrame->getLinePtr(y, (Pixel*)0);
				if (colour != colour2) {
					srcEndY = y;
					dstEndY = dstStartY + lineZoom * (srcEndY - srcStartY);
					if (dstEndY > dstHeight) dstEndY = dstHeight;
					break;
				}
			}
			// TODO: This isn't 100% accurate in case of deinterlace:
			//       on the previous frame, this area may have contained
			//       graphics instead of blank pixels.
			currScaler->scaleBlank(colour, screen, dstStartY, dstEndY);
		} else {
			if (deinterlace) {
				currScaler->scaleImage(
					*frameEven, *frameOdd, lineWidth,
					screen, srcStartY, srcEndY
					);
			} else {
				currScaler->scaleImage(
					*currFrame, lineWidth, srcStartY, srcEndY, // source
					screen, dstStartY, dstEndY // dest
					);
			}
		}

		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	srcStartY, srcEndY, lineWidth );
		srcStartY = srcEndY;
		dstStartY = dstEndY;
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
	RawFrame* finishedFrame, RawFrame::FieldType field)
{
	RawFrame* reuseFrame = prevFrame;
	prevFrame = currFrame;
	currFrame = finishedFrame;
	reuseFrame->init(field);
	return reuseFrame;
}


// Force template instantiation.
template class PostProcessor<Uint16>;
template class PostProcessor<Uint32>;

} // namespace openmsx
