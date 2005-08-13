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
PostProcessor<Pixel>::PostProcessor(SDL_Surface* screen)
	: VideoLayer(VIDEO_MSX)
{
	this->screen = screen;
	currScalerID = (ScalerID)-1; // not a valid scaler

	// Note: Since these frames contain just black lines, there is no point
	//       in getting accurate interlace info.
	currFrame = new RawFrame(screen->format, RawFrame::FIELD_NONINTERLACED);
	prevFrame = new RawFrame(screen->format, RawFrame::FIELD_NONINTERLACED);
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
	const bool deinterlace =
		field != RawFrame::FIELD_NONINTERLACED
		&& RenderSettings::instance().getDeinterlace().getValue();

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
		case 256:
			if (deinterlace) {
				RawFrame::FieldType field = currFrame->getField();
				assert(field != RawFrame::FIELD_NONINTERLACED);
				bool odd = field == RawFrame::FIELD_ODD;
				currScaler->scale256(
					*(odd ? prevFrame : currFrame), // even
					*(odd ? currFrame : prevFrame), // odd
					screen, startY, endY);
					
			} else {
				currScaler->scale256(*currFrame, screen,
				                     startY, endY, lower);
			}
			break;
		case 512:
			if (deinterlace) {
				RawFrame::FieldType field = currFrame->getField();
				assert(field != RawFrame::FIELD_NONINTERLACED);
				bool odd = field == RawFrame::FIELD_ODD;
				currScaler->scale512(
					*(odd ? prevFrame : currFrame), // even
					*(odd ? currFrame : prevFrame), // odd
					screen, startY, endY);
					
			} else {
				currScaler->scale512(*currFrame, screen,
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
	// TODO: A more useful name, maybe parameter of constructor?
	static const std::string NAME = "PostProcessor";
	return NAME;
}

template <class Pixel>
RawFrame* PostProcessor<Pixel>::rotateFrames(
	RawFrame* finishedFrame, RawFrame::FieldType field )
{
	RawFrame* reuseFrame = prevFrame;
	prevFrame = currFrame;
	currFrame = finishedFrame
		? finishedFrame
		: new RawFrame(screen->format, RawFrame::FIELD_NONINTERLACED);
	reuseFrame->reinit(field);
	return reuseFrame;
}


// Force template instantiation.
template class PostProcessor<Uint16>;
template class PostProcessor<Uint32>;

} // namespace openmsx
