// $Id:$

#include "StretchScalerOutput.hh"
#include "DirectScalerOutput.hh"
#include "LineScalers.hh"
#include "PixelOperations.hh"
#include "MemoryOps.hh"
#include "build-info.hh"
#include <vector>

using std::auto_ptr;

namespace openmsx {


template<typename Pixel>
class StretchScalerOutput : public ScalerOutput<Pixel>
{
public:
	StretchScalerOutput(auto_ptr<ScalerOutput<Pixel> >& output,
	                    const PixelOperations<Pixel>& pixelOps,
	                    unsigned inWidth);
	~StretchScalerOutput();

	virtual unsigned getWidth()  const;
	virtual unsigned getHeight() const;
	virtual Pixel* acquireLine(unsigned y);
	virtual void   releaseLine(unsigned y, Pixel* buf);
	virtual void   fillLine   (unsigned y, Pixel color);

private:
	auto_ptr<ScalerOutput<Pixel> > output;
	const PixelOperations<Pixel> pixelOps;
	unsigned inWidth;
	std::vector<Pixel*> pool;
};


template<typename Pixel>
StretchScalerOutput<Pixel>::StretchScalerOutput(
		auto_ptr<ScalerOutput<Pixel> >& output_,
		const PixelOperations<Pixel>& pixelOps_,
		unsigned inWidth_)
	: output(output_)
	, pixelOps(pixelOps_)
	, inWidth(inWidth_)
{
}

template<typename Pixel>
StretchScalerOutput<Pixel>::~StretchScalerOutput()
{
	for (typename std::vector<Pixel*>::iterator it = pool.begin();
	     it != pool.end(); ++it) {
		MemoryOps::freeAligned(*it);
	}
}

template<typename Pixel>
unsigned StretchScalerOutput<Pixel>::getWidth() const
{
	return output->getWidth();
}

template<typename Pixel>
unsigned StretchScalerOutput<Pixel>::getHeight() const
{
	return output->getHeight();
}

template<typename Pixel>
Pixel* StretchScalerOutput<Pixel>::acquireLine(unsigned /*y*/)
{
	if (!pool.empty()) {
		Pixel* buf = pool.back();
		pool.pop_back();
		return buf;
	} else {
		unsigned size = sizeof(Pixel) * output->getWidth();
		Pixel* buf = static_cast<Pixel*>(MemoryOps::mallocAligned(64, size));
		return buf;
	}
}

template<typename Pixel>
void StretchScalerOutput<Pixel>::releaseLine(unsigned y, Pixel* buf)
{
	pool.push_back(buf);
	Pixel* dstLine = output->acquireLine(y);

	unsigned dstWidth = output->getWidth();
	unsigned srcWidth = (dstWidth / 320) * inWidth;
	unsigned srcOffset = (dstWidth - srcWidth) / 2;
	ZoomLine<Pixel> zoom(pixelOps);
	zoom(buf + srcOffset, srcWidth, dstLine, dstWidth);

	output->releaseLine(y, dstLine);
}

template<typename Pixel>
void StretchScalerOutput<Pixel>::fillLine(unsigned y, Pixel color)
{
	Pixel* dstLine = output->acquireLine(y);
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	memset(dstLine, output->getWidth(), color);
	output->releaseLine(y, dstLine);
}


// class StretchScalerOutputFactory

template<typename Pixel>
auto_ptr<ScalerOutput<Pixel> > StretchScalerOutputFactory<Pixel>::create(
	OutputSurface& output,
	const PixelOperations<Pixel>& pixelOps,
	unsigned inWidth)
{
	auto_ptr<ScalerOutput<Pixel> > direct(
		new DirectScalerOutput<Pixel>(output));
	if (inWidth == 320) {
		return direct;
	} else {
		return auto_ptr<ScalerOutput<Pixel> >(
			new StretchScalerOutput<Pixel>(
				direct, pixelOps, inWidth));
	}
}

// Force template instantiation.
#if HAVE_16BPP
template class StretchScalerOutputFactory<word>;
#endif
#if HAVE_32BPP
template class StretchScalerOutputFactory<unsigned>;
#endif

} // namespace openmsx
