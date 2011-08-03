// $Id$

#include "FrameSource.hh"
#include "PixelOperations.hh"
#include "MemoryOps.hh"
#include "LineScalers.hh"
#include "openmsx.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include "components.hh"

namespace openmsx {

FrameSource::FrameSource(const SDL_PixelFormat& format)
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
const Pixel* FrameSource::blendLines(
	const Pixel* line1, const Pixel* line2, unsigned width) const
{
	PixelOperations<Pixel> pixelOps(pixelFormat);
	BlendLines<Pixel> blend(pixelOps);
	Pixel* out = reinterpret_cast<Pixel*>(getTempBuffer());
	blend(line1, line2, out, width);
	return out;
}

template <typename Pixel>
const Pixel* FrameSource::getLinePtr320_240(unsigned line) const
{
	if (getHeight() == 240) {
		return getLinePtr<Pixel>(line, 320);
	} else {
		assert(getHeight() == 480);
		const Pixel* line1 = getLinePtr<Pixel>(2 * line + 0, 320);
		const Pixel* line2 = getLinePtr<Pixel>(2 * line + 1, 320);
		return blendLines(line1, line2, 320);
	}
}

template <typename Pixel>
const Pixel* FrameSource::getLinePtr640_480(unsigned line) const
{
	if (getHeight() == 480) {
		return getLinePtr<Pixel>(line, 640);
	} else {
		assert(getHeight() == 240);
		return getLinePtr<Pixel>(line / 2, 640);
	}
}

template <typename Pixel>
const Pixel* FrameSource::getLinePtr960_720(unsigned line) const
{
	if (getHeight() == 480) {
		unsigned l2 = (2 * line) / 3;
		const Pixel* line0 = getLinePtr<Pixel>(l2 + 0, 960);
		if ((line % 3) == 1) {
			const Pixel* line1 = getLinePtr<Pixel>(l2 + 1, 960);
			return blendLines(line0, line1, 960);
		} else {
			return line0;
		}
	} else {
		assert(getHeight() == 240);
		return getLinePtr<Pixel>(line / 3, 960);
	}
}

void* FrameSource::getTempBuffer() const
{
	if (tempCounter == tempBuffers.size()) {
		unsigned size = 1280 * pixelFormat.BytesPerPixel;
		void* buf = MemoryOps::mallocAligned(64, size);
		tempBuffers.push_back(buf);
	}
	return tempBuffers[tempCounter++];
}

void FrameSource::freeLineBuffers() const
{
	tempCounter = 0; // reuse tempBuffers
}

template <typename Pixel>
const Pixel* FrameSource::scaleLine(
		const Pixel* in, unsigned inWidth, unsigned outWidth) const
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
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
			UNREACHABLE;
		default:
			UNREACHABLE;
		}
		break;
	default:
		UNREACHABLE;
	}

	return out;
}


// Force template method instantiation
#if HAVE_16BPP
template const word* FrameSource::getLinePtr320_240<word>(unsigned) const;
template const word* FrameSource::getLinePtr640_480<word>(unsigned) const;
template const word* FrameSource::getLinePtr960_720<word>(unsigned) const;
template const word* FrameSource::scaleLine<word>(const word*, unsigned, unsigned) const;
#endif
#if HAVE_32BPP || COMPONENT_GL
template const unsigned* FrameSource::getLinePtr320_240<unsigned>(unsigned) const;
template const unsigned* FrameSource::getLinePtr640_480<unsigned>(unsigned) const;
template const unsigned* FrameSource::getLinePtr960_720<unsigned>(unsigned) const;
template const unsigned* FrameSource::scaleLine<unsigned>(const unsigned*, unsigned, unsigned) const;
#endif

} // namespace openmsx
