// $Id$

#include "FrameSource.hh"
#include "PixelOperations.hh"
#include "MemoryOps.hh"
#include "LineScalers.hh"
#include "openmsx.hh"
#include <iostream>

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
}

void FrameSource::setHeight(unsigned height_)
{
	height = height_;
}

template <typename Pixel>
const Pixel* FrameSource::getLinePtr320_240(unsigned line, Pixel* dummy)
{
	if (getHeight() == 240) {
		return getLinePtr(line, 320, dummy);
	} else {
		assert(getHeight() == 480);
		const Pixel* line1 = getLinePtr(2 * line + 0, 320, dummy);
		const Pixel* line2 = getLinePtr(2 * line + 1, 320, dummy);
		PixelOperations<Pixel> pixelOps(pixelFormat);
		BlendLines<Pixel> blend(pixelOps);
		Pixel* out = reinterpret_cast<Pixel*>(getTempBuffer());
		blend(line1, line2, out, 320);
		return out;
	}
}

void* FrameSource::getTempBuffer()
{
	if (tempCounter == tempBuffers.size()) {
		void* buf = MemoryOps::mallocAligned(64, getLineBufferSize());
		tempBuffers.push_back(buf);
	}
	return tempBuffers[tempCounter++];
}

void FrameSource::freeLineBuffers()
{
	tempCounter = 0; // reuse tempBuffers
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
		MemoryOps::MemSet<Pixel, MemoryOps::NO_STREAMING> memset;
		memset(out, outWidth, in[0]);
		break;
	case 213:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213:
			assert(false);
		case 320: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426: {
			Scale_1on2<Pixel, false> scale; // no streaming
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_1on3<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 853: {
			Scale_1on4<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_2on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_1on6<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		default:
			assert(false);
		}
		break;
	case 320:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320:
			assert(false);
		case 426: {
			Scale_3on4<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_1on2<Pixel, false> scale; // no streaming
			scale(in, out, outWidth);
			break;
		}
		case 853: {
			Scale_3on8<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_1on3<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_1on4<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		default:
			assert(false);
		}
		break;
	case 426:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426:
			assert(false);
		case 640: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 853: {
			Scale_1on2<Pixel, false> scale; // no streaming
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_4on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_1on3<Pixel> scale;
			scale(in, out, outWidth);
			break;
		}
		default:
			assert(false);
		}
		break;
	case 640:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_3on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640:
			assert(false);
		case 853: {
			Scale_3on4<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_1on2<Pixel, false> scale; // no streaming
			scale(in, out, outWidth);
			break;
		}
		default:
			assert(false);
		}
		break;
	case 853:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_4on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320: {
			Scale_8on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 853:
			assert(false);
		case 960: {
			Scale_8on9<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280: {
			Scale_2on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		default:
			assert(false);
		}
		break;
	case 1280:
		switch (outWidth) {
		case 1:
			out[0] = in[0];
			break;
		case 213: {
			Scale_6on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 320: {
			Scale_4on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 426: {
			Scale_3on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 640: {
			Scale_2on1<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 853: {
			Scale_3on2<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 960: {
			Scale_4on3<Pixel> scale(pixelOps);
			scale(in, out, outWidth);
			break;
		}
		case 1280:
			assert(false);
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
template const word* FrameSource::getLinePtr320_240(unsigned, word*);
template const unsigned* FrameSource::getLinePtr320_240(unsigned, unsigned*);

} // namespace openmsx
