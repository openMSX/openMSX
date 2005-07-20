// $Id$

#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "Scaler.hh"
#include "BooleanSetting.hh"
#include "MemoryOps.hh"
#include <SDL.h>
#include <cassert>


namespace openmsx {

template <class Pixel, Renderer::Zoom zoom>
PostProcessor<Pixel, zoom>::PostProcessor(SDL_Surface* screen)
	: VideoLayer(VIDEO_MSX)
	, blender(Blender<Pixel>::createFromFormat(screen->format))
{
	this->screen = screen;
	currScalerID = (ScalerID)-1; // not a valid scaler

	// Note: Since these frames contain just black lines, there is no point
	//       in getting accurate interlace info.
	currFrame = new RawFrame(screen->format, RawFrame::FIELD_NONINTERLACED);
	prevFrame = new RawFrame(screen->format, RawFrame::FIELD_NONINTERLACED);
}

template <class Pixel, Renderer::Zoom zoom>
PostProcessor<Pixel, zoom>::~PostProcessor()
{
	delete currFrame;
	delete prevFrame;
}

template <class Pixel, Renderer::Zoom zoom>
void PostProcessor<Pixel, zoom>::paint()
{
	// All of the current postprocessing steps require hi-res.
	if (LINE_ZOOM != 2) {
		// Just copy the image as-is.
		// TODO: This is incorrect, since the image is now always 640 wide.
		if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		SDL_BlitSurface(currFrame->getSurface(), NULL, screen, NULL);
		if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
		return;
	}

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
	//       frame, we should take the content of two frames into account.
	const unsigned deltaY = field == RawFrame::FIELD_ODD ? 1 : 0;
	unsigned startY = 0;
	while (startY < HEIGHT / 2) {
		const RawFrame::LineContent content = currFrame->getLineContent(startY);
		unsigned endY = startY + 1;
		while (endY < HEIGHT / 2
			&& currFrame->getLineContent(endY) == content) endY++;

		switch (content) {
		case RawFrame::LINE_BLANK: {
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
				// TODO: Why add deltaY when deinterlacing?
				currScaler->scaleBlank(
					colour, screen,
					startY * 2 + deltaY, endY * 2 + deltaY );
			} else {
				currScaler->scaleBlank(
					colour, screen,
					startY * 2 + deltaY, endY * 2 + deltaY );
			}
			break;
		}
		case RawFrame::LINE_256:
			if (deinterlace) {
				for (unsigned y = startY; y < endY; y++) {
					RawFrame::FieldType field = currFrame->getField();
					assert(field != RawFrame::FIELD_NONINTERLACED);
					bool odd = field == RawFrame::FIELD_ODD;
					deinterlacer.deinterlaceLine256(
						(odd ? prevFrame : currFrame)->getSurface(), // even
						(odd ? currFrame : prevFrame)->getSurface(), // odd
						y, screen, y * 2
						);
				}
			} else {
				currScaler->scale256(
					currFrame->getSurface(),
					startY, endY, screen, startY * 2 + deltaY
					);
			}
			break;
		case RawFrame::LINE_512:
			if (deinterlace) {
				for (unsigned y = startY; y < endY; y++) {
					RawFrame::FieldType field = currFrame->getField();
					assert(field != RawFrame::FIELD_NONINTERLACED);
					bool odd = field == RawFrame::FIELD_ODD;
					deinterlacer.deinterlaceLine512(
						(odd ? prevFrame : currFrame)->getSurface(), // even
						(odd ? currFrame : prevFrame)->getSurface(), // odd
						y, screen, y * 2
						);
				}
			} else {
				currScaler->scale512(
					currFrame->getSurface(),
					startY, endY, screen, startY * 2 + deltaY
					);
			}
			break;
		default:
			assert(false);
			break;
		}

		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	startY, endY, content );
		startY = endY;
	}
}

template <class Pixel, Renderer::Zoom zoom>
const std::string& PostProcessor<Pixel, zoom>::getName()
{
	// TODO: A more useful name, maybe parameter of constructor?
	static const std::string NAME = "PostProcessor";
	return NAME;
}

template <class Pixel, Renderer::Zoom zoom>
RawFrame* PostProcessor<Pixel, zoom>::rotateFrames(
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
template class PostProcessor<Uint16, Renderer::ZOOM_256>;
template class PostProcessor<Uint16, Renderer::ZOOM_REAL>;
template class PostProcessor<Uint32, Renderer::ZOOM_256>;
template class PostProcessor<Uint32, Renderer::ZOOM_REAL>;

} // namespace openmsx
