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
class StretchScalerOutputBase : public ScalerOutput<Pixel>
{
public:
	StretchScalerOutputBase(auto_ptr<ScalerOutput<Pixel> >& output,
	                        const PixelOperations<Pixel>& pixelOps);
	~StretchScalerOutputBase();

	virtual unsigned getWidth()  const;
	virtual unsigned getHeight() const;
	virtual Pixel* acquireLine(unsigned y);
	virtual void   releaseLine(unsigned y, Pixel* buf) = 0;
	virtual void   fillLine   (unsigned y, Pixel color);

protected:
	Pixel* releasePre(unsigned y, Pixel* buf);
	void releasePost(unsigned y, Pixel* dstLine);

	const PixelOperations<Pixel> pixelOps;

private:
	auto_ptr<ScalerOutput<Pixel> > output;
	std::vector<Pixel*> pool;
};

template<typename Pixel>
class StretchScalerOutput : public StretchScalerOutputBase<Pixel>
{
public:
	StretchScalerOutput(auto_ptr<ScalerOutput<Pixel> >& output,
	                    const PixelOperations<Pixel>& pixelOps,
	                    unsigned inWidth);
	virtual void releaseLine(unsigned y, Pixel* buf);

private:
	unsigned inWidth;
};

template<typename Pixel, unsigned IN_WIDTH, typename SCALE>
class StretchScalerOutputN : public StretchScalerOutputBase<Pixel>
{
public:
	StretchScalerOutputN(auto_ptr<ScalerOutput<Pixel> >& output,
	                       const PixelOperations<Pixel>& pixelOps);
	virtual void releaseLine(unsigned y, Pixel* buf);
};

template<typename Pixel>
class StretchScalerOutput256
	: public StretchScalerOutputN<Pixel, 256, Scale_4on5<Pixel> >
{
public:
	StretchScalerOutput256(auto_ptr<ScalerOutput<Pixel> >& output,
	                       const PixelOperations<Pixel>& pixelOps);
};

template<typename Pixel>
class StretchScalerOutput272
	: public StretchScalerOutputN<Pixel, 272, Scale_17on20<Pixel> >
{
public:
	StretchScalerOutput272(auto_ptr<ScalerOutput<Pixel> >& output,
	                       const PixelOperations<Pixel>& pixelOps);
};

template<typename Pixel>
class StretchScalerOutput280
	: public StretchScalerOutputN<Pixel, 280, Scale_7on8<Pixel> >
{
public:
	StretchScalerOutput280(auto_ptr<ScalerOutput<Pixel> >& output,
	                       const PixelOperations<Pixel>& pixelOps);
};

template<typename Pixel>
class StretchScalerOutput288
	: public StretchScalerOutputN<Pixel, 288, Scale_9on10<Pixel> >
{
public:
	StretchScalerOutput288(auto_ptr<ScalerOutput<Pixel> >& output,
	                       const PixelOperations<Pixel>& pixelOps);
};


// class StretchScalerOutputBase

template<typename Pixel>
StretchScalerOutputBase<Pixel>::StretchScalerOutputBase(
		auto_ptr<ScalerOutput<Pixel> >& output_,
		const PixelOperations<Pixel>& pixelOps_)
	: pixelOps(pixelOps_)
	, output(output_)
{
}

template<typename Pixel>
StretchScalerOutputBase<Pixel>::~StretchScalerOutputBase()
{
	for (typename std::vector<Pixel*>::iterator it = pool.begin();
	     it != pool.end(); ++it) {
		MemoryOps::freeAligned(*it);
	}
}

template<typename Pixel>
unsigned StretchScalerOutputBase<Pixel>::getWidth() const
{
	return output->getWidth();
}

template<typename Pixel>
unsigned StretchScalerOutputBase<Pixel>::getHeight() const
{
	return output->getHeight();
}

template<typename Pixel>
Pixel* StretchScalerOutputBase<Pixel>::acquireLine(unsigned /*y*/)
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
Pixel* StretchScalerOutputBase<Pixel>::releasePre(unsigned y, Pixel* buf)
{
	pool.push_back(buf);
	return output->acquireLine(y);
}

template<typename Pixel>
void StretchScalerOutputBase<Pixel>::releasePost(unsigned y, Pixel* dstLine)
{
	output->releaseLine(y, dstLine);
}

template<typename Pixel>
void StretchScalerOutputBase<Pixel>::fillLine(unsigned y, Pixel color)
{
	Pixel* dstLine = output->acquireLine(y);
	MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
	memset(dstLine, output->getWidth(), color);
	output->releaseLine(y, dstLine);
}


// class StretchScalerOutput

template<typename Pixel>
StretchScalerOutput<Pixel>::StretchScalerOutput(
		auto_ptr<ScalerOutput<Pixel> >& output,
		const PixelOperations<Pixel>& pixelOps,
		unsigned inWidth_)
	: StretchScalerOutputBase<Pixel>(output, pixelOps)
	, inWidth(inWidth_)
{
}

template<typename Pixel>
void StretchScalerOutput<Pixel>::releaseLine(unsigned y, Pixel* buf)
{
	Pixel* dstLine = this->releasePre(y, buf);

	unsigned dstWidth = StretchScalerOutputBase<Pixel>::getWidth();
	unsigned srcWidth = (dstWidth / 320) * inWidth;
	unsigned srcOffset = (dstWidth - srcWidth) / 2;
	ZoomLine<Pixel> zoom(this->pixelOps);
	zoom(buf + srcOffset, srcWidth, dstLine, dstWidth);

	this->releasePost(y, dstLine);
}


// class StretchScalerOutputN

template<typename Pixel, unsigned IN_WIDTH, typename SCALE>
StretchScalerOutputN<Pixel, IN_WIDTH, SCALE>::StretchScalerOutputN(
		auto_ptr<ScalerOutput<Pixel> >& output,
		const PixelOperations<Pixel>& pixelOps)
	: StretchScalerOutputBase<Pixel>(output, pixelOps)
{
}

template<typename Pixel, unsigned IN_WIDTH, typename SCALE>
void StretchScalerOutputN<Pixel, IN_WIDTH, SCALE>::releaseLine(unsigned y, Pixel* buf)
{
	Pixel* dstLine = this->releasePre(y, buf);

	unsigned dstWidth = StretchScalerOutputBase<Pixel>::getWidth();
	unsigned srcWidth = (dstWidth / 320) * IN_WIDTH;
	unsigned srcOffset = (dstWidth - srcWidth) / 2;
	SCALE scale(this->pixelOps);
	scale(buf + srcOffset, dstLine, dstWidth);

	this->releasePost(y, dstLine);
}


// class StretchScalerOutput256

template<typename Pixel>
StretchScalerOutput256<Pixel>::StretchScalerOutput256(
		auto_ptr<ScalerOutput<Pixel> >& output,
		const PixelOperations<Pixel>& pixelOps)
	: StretchScalerOutputN<Pixel, 256, Scale_4on5<Pixel> >(output, pixelOps)
{
}


// class StretchScalerOutput272

template<typename Pixel>
StretchScalerOutput272<Pixel>::StretchScalerOutput272(
		auto_ptr<ScalerOutput<Pixel> >& output,
		const PixelOperations<Pixel>& pixelOps)
	: StretchScalerOutputN<Pixel, 272, Scale_17on20<Pixel> >(output, pixelOps)
{
}


// class StretchScalerOutput280

template<typename Pixel>
StretchScalerOutput280<Pixel>::StretchScalerOutput280(
		auto_ptr<ScalerOutput<Pixel> >& output,
		const PixelOperations<Pixel>& pixelOps)
	: StretchScalerOutputN<Pixel, 280, Scale_7on8<Pixel> >(output, pixelOps)
{
}


// class StretchScalerOutput288

template<typename Pixel>
StretchScalerOutput288<Pixel>::StretchScalerOutput288(
		auto_ptr<ScalerOutput<Pixel> >& output,
		const PixelOperations<Pixel>& pixelOps)
	: StretchScalerOutputN<Pixel, 288, Scale_9on10<Pixel> >(output, pixelOps)
{
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
	switch (inWidth) {
	case 320:
		return direct;
	case 288:
		return auto_ptr<ScalerOutput<Pixel> >(
			new StretchScalerOutput288<Pixel>(
				direct, pixelOps));
	case 280:
		return auto_ptr<ScalerOutput<Pixel> >(
			new StretchScalerOutput280<Pixel>(
				direct, pixelOps));
	case 272:
		return auto_ptr<ScalerOutput<Pixel> >(
			new StretchScalerOutput272<Pixel>(
				direct, pixelOps));
	case 256:
		return auto_ptr<ScalerOutput<Pixel> >(
			new StretchScalerOutput256<Pixel>(
				direct, pixelOps));
	default:
		return auto_ptr<ScalerOutput<Pixel> >(
			new StretchScalerOutput<Pixel>(
				direct, pixelOps, inWidth));
	}
}

// Force template instantiation.
#if HAVE_16BPP
template struct StretchScalerOutputFactory<word>;
#endif
#if HAVE_32BPP
template struct StretchScalerOutputFactory<unsigned>;
#endif

} // namespace openmsx
