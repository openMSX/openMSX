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
PostProcessor<Pixel>::PostProcessor(SDL_Surface* screen, VideoSource videoSource,
                                    unsigned maxWidth)
	: VideoLayer(videoSource)
{
	this->screen = screen;
	currScalerID = (ScalerID)-1; // not a valid scaler

	currFrame = new RawFrame(screen->format, maxWidth);
	prevFrame = new RawFrame(screen->format, maxWidth);
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
                        RenderSettings::instance().getDeinterlace().getValue();
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
	ScalerID scalerID = RenderSettings::instance().getScaler().getValue();
	if (currScalerID != scalerID) {
		currScaler = Scaler<Pixel>::createScaler(scalerID, screen->format);
		currScalerID = scalerID;
	}

	// Scale image.
	// TODO: Check deinterlace first: now that we keep LineContent info per
	//       frame, we should take the lineWidth of two frames into account.
	bool lower = field == RawFrame::FIELD_ODD;
	unsigned startY = 0;
	while (startY < 240) {
		unsigned lineWidth = currFrame->getLineWidth(startY);
		unsigned endY = startY + 1;
		while ((endY < 240) &&
		       (currFrame->getLineWidth(endY) == lineWidth)) endY++;

		switch (lineWidth) {
		case 0: { // blank line
			// Reduce area to same-colour starting segment.
			const Pixel colour = *currFrame->getPixelPtr(0, startY, (Pixel*)0);
			for (unsigned y = startY + 1; y < endY; y++) {
				const Pixel colour2 = *currFrame->getPixelPtr(0, y, (Pixel*)0);
				if (colour != colour2) endY = y;
			}

			if (deinterlace) {
				// TODO: This isn't 100% accurate:
				//       on the previous frame, this area may have contained
				//       graphics instead of blank pixels.
				currScaler->scaleBlank(colour, screen,
				                       startY, endY, false);
			} else {
				currScaler->scaleBlank(colour, screen,
				                       startY, endY, lower);
			}
			break;
		}
		// TODO find a better way to implement this
		case 192:
			if (deinterlace) {
				currScaler->scale192(*frameEven, *frameOdd,
				                     screen, startY, endY);
			} else {
				currScaler->scale192(*currFrame, screen,
				                     startY, endY, lower);
			}
			break;
		case 256:
			if (deinterlace) {
				currScaler->scale256(*frameEven, *frameOdd,
				                     screen, startY, endY);
			} else {
				currScaler->scale256(*currFrame, screen,
				                     startY, endY, lower);
			}
			break;
		case 384:
			if (deinterlace) {
				currScaler->scale384(*frameEven, *frameOdd,
				                     screen, startY, endY);
			} else {
				currScaler->scale384(*currFrame, screen,
				                     startY, endY, lower);
			}
			break;
		case 512:
			if (deinterlace) {
				currScaler->scale512(*frameEven, *frameOdd,
				                     screen, startY, endY);
			} else {
				currScaler->scale512(*currFrame, screen,
				                     startY, endY, lower);
			}
			break;
		case 640:
			if (deinterlace) {
				currScaler->scale640(*frameEven, *frameOdd,
				                     screen, startY, endY);
			} else {
				currScaler->scale640(*currFrame, screen,
				                     startY, endY, lower);
			}
			break;
		case 768:
			if (deinterlace) {
				currScaler->scale768(*frameEven, *frameOdd,
				                     screen, startY, endY);
			} else {
				currScaler->scale768(*currFrame, screen,
				                     startY, endY, lower);
			}
			break;
		case 1024:
			if (deinterlace) {
				currScaler->scale1024(*frameEven, *frameOdd,
				                      screen, startY, endY);
			} else {
				currScaler->scale1024(*currFrame, screen,
				                      startY, endY, lower);
			}
			break;
		default:
			assert(false);
			break;
		}

		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	startY, endY, lineWidth );
		startY = endY;
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
	reuseFrame->reinit(field);
	return reuseFrame;
}


// Force template instantiation.
template class PostProcessor<Uint16>;
template class PostProcessor<Uint32>;

} // namespace openmsx
