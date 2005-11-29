// $Id$

#include "FrameSource.hh"
#include "PixelOperations.hh"
#include "MemoryOps.hh"
#include "LineScalers.hh"
#include "openmsx.hh"

namespace openmsx {

FrameSource::FrameSource(const SDL_PixelFormat* format)
	: pixelFormat(format)
	, tempCounter(0)
{
}

FrameSource::~FrameSource()
{
	for (std::vector<void*>::const_iterator it = tempBuffers.begin();
	     it != tempBuffers.end(); ++it) {
		MemoryOps::freeAligned(*it);
	}
}

void FrameSource::init(FieldType fieldType_)
{
	fieldType = fieldType_;
	tempCounter = 0; // reuse tempBuffers
}

void FrameSource::setHeight(unsigned height_)
{
	height = height_;
}

void* FrameSource::getTempBuffer()
{
	if (tempCounter == tempBuffers.size()) {
		void* buf = MemoryOps::mallocAligned(64, getLineBufferSize());
		tempBuffers.push_back(buf);
	}
	return tempBuffers[tempCounter++];
}

template <typename Pixel>
const Pixel* FrameSource::scaleLine(
		const Pixel* in, unsigned inWidth, unsigned outWidth)
{
	PixelOperations<Pixel> pixelOps(pixelFormat);
	Pixel* out = reinterpret_cast<Pixel*>(getTempBuffer());

	// TODO is there a better way to implement this?
	switch (inWidth) {
	case 1:  // blank
		MemoryOps::memset<Pixel, MemoryOps::NO_STREAMING>(
			out, outWidth, in[0]);
		break;
	case 213:
		switch (outWidth) {
		case 320: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_1on3<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_2on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		default:
			  assert(false);
		}
		break;
	case 320:
		switch (outWidth) {
		case 320:
			assert(false);
		case 640: {
			Scale_1on2<Pixel, false> scale; // no streaming
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_1on3<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		default:
			  assert(false);
		}
		break;
	case 426:
		switch (outWidth) {
		case 320: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_4on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		default:
			  assert(false);
		}
		break;
	case 640:
		switch (outWidth) {
		case 320: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640:
			assert(false);
		case 960: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		default:
			  assert(false);
		}
		break;
	case 853:
		switch (outWidth) {
		case 320: {
			Scale_8on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_8on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		default:
			  assert(false);
		}
		break;
	case 1280:
		switch (outWidth) {
		case 320: {
			Scale_4on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		default:
			  assert(false);
		}
		break;
	default:
		assert(false);
	}

	return out;
}


// Force template method instantiation
template const word* FrameSource::scaleLine(const word*, unsigned, unsigned);
template const unsigned* FrameSource::scaleLine(const unsigned*, unsigned, unsigned);

} // namespace openmsx
