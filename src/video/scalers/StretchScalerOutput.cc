#include "StretchScalerOutput.hh"
#include "DirectScalerOutput.hh"
#include "LineScalers.hh"
#include "PixelOperations.hh"
#include "MemoryOps.hh"
#include "build-info.hh"
#include <cstdint>
#include <memory>
#include <vector>

using std::unique_ptr;

namespace openmsx {

template<typename Pixel>
class StretchScalerOutputBase : public ScalerOutput<Pixel>
{
public:
	StretchScalerOutputBase(unique_ptr<ScalerOutput<Pixel>> output,
	                        PixelOperations<Pixel> pixelOps);
	~StretchScalerOutputBase() override;

	unsigned getWidth()  const override;
	unsigned getHeight() const override;
	Pixel* acquireLine(unsigned y) override;
	void   fillLine   (unsigned y, Pixel color) override;

protected:
	Pixel* releasePre(unsigned y, Pixel* buf);
	void releasePost(unsigned y, Pixel* dstLine);

	const PixelOperations<Pixel> pixelOps;

private:
	unique_ptr<ScalerOutput<Pixel>> output;
	std::vector<Pixel*> pool;
};

template<typename Pixel>
class StretchScalerOutput : public StretchScalerOutputBase<Pixel>
{
public:
	StretchScalerOutput(unique_ptr<ScalerOutput<Pixel>> output,
	                    PixelOperations<Pixel> pixelOps,
	                    unsigned inWidth);
	void releaseLine(unsigned y, Pixel* buf) override;

private:
	unsigned inWidth;
};

template<typename Pixel, unsigned IN_WIDTH, typename SCALE>
class StretchScalerOutputN : public StretchScalerOutputBase<Pixel>
{
public:
	StretchScalerOutputN(unique_ptr<ScalerOutput<Pixel>> output,
	                     PixelOperations<Pixel> pixelOps);
	void releaseLine(unsigned y, Pixel* buf) override;
};

template<typename Pixel>
class StretchScalerOutput256
	: public StretchScalerOutputN<Pixel, 256, Scale_4on5<Pixel>>
{
public:
	StretchScalerOutput256(unique_ptr<ScalerOutput<Pixel>> output,
	                       PixelOperations<Pixel> pixelOps);
};

template<typename Pixel>
class StretchScalerOutput272
	: public StretchScalerOutputN<Pixel, 272, Scale_17on20<Pixel>>
{
public:
	StretchScalerOutput272(unique_ptr<ScalerOutput<Pixel>> output,
	                       PixelOperations<Pixel> pixelOps);
};

template<typename Pixel>
class StretchScalerOutput280
	: public StretchScalerOutputN<Pixel, 280, Scale_7on8<Pixel>>
{
public:
	StretchScalerOutput280(unique_ptr<ScalerOutput<Pixel>> output,
	                       PixelOperations<Pixel> pixelOps);
};

template<typename Pixel>
class StretchScalerOutput288
	: public StretchScalerOutputN<Pixel, 288, Scale_9on10<Pixel>>
{
public:
	StretchScalerOutput288(unique_ptr<ScalerOutput<Pixel>> output,
	                       PixelOperations<Pixel> pixelOps);
};


// class StretchScalerOutputBase

template<typename Pixel>
StretchScalerOutputBase<Pixel>::StretchScalerOutputBase(
		unique_ptr<ScalerOutput<Pixel>> output_,
		PixelOperations<Pixel> pixelOps_)
	: pixelOps(std::move(pixelOps_))
	, output(std::move(output_))
{
}

template<typename Pixel>
StretchScalerOutputBase<Pixel>::~StretchScalerOutputBase()
{
	for (auto& p : pool) {
		MemoryOps::freeAligned(p);
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
		return static_cast<Pixel*>(MemoryOps::mallocAligned(64, size));
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
	MemoryOps::MemSet<Pixel> memset;
	memset(dstLine, output->getWidth(), color);
	output->releaseLine(y, dstLine);
}


// class StretchScalerOutput

template<typename Pixel>
StretchScalerOutput<Pixel>::StretchScalerOutput(
		unique_ptr<ScalerOutput<Pixel>> output_,
		PixelOperations<Pixel> pixelOps_,
		unsigned inWidth_)
	: StretchScalerOutputBase<Pixel>(std::move(output_), std::move(pixelOps_))
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
		unique_ptr<ScalerOutput<Pixel>> output_,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputBase<Pixel>(std::move(output_), std::move(pixelOps_))
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
		unique_ptr<ScalerOutput<Pixel>> output_,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputN<Pixel, 256, Scale_4on5<Pixel>>(
		std::move(output_), std::move(pixelOps_))
{
}


// class StretchScalerOutput272

template<typename Pixel>
StretchScalerOutput272<Pixel>::StretchScalerOutput272(
		unique_ptr<ScalerOutput<Pixel>> output_,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputN<Pixel, 272, Scale_17on20<Pixel>>(
		std::move(output_), std::move(pixelOps_))
{
}


// class StretchScalerOutput280

template<typename Pixel>
StretchScalerOutput280<Pixel>::StretchScalerOutput280(
		unique_ptr<ScalerOutput<Pixel>> output_,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputN<Pixel, 280, Scale_7on8<Pixel>>(
		std::move(output_), std::move(pixelOps_))
{
}


// class StretchScalerOutput288

template<typename Pixel>
StretchScalerOutput288<Pixel>::StretchScalerOutput288(
		unique_ptr<ScalerOutput<Pixel>> output_,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputN<Pixel, 288, Scale_9on10<Pixel>>(
		std::move(output_), std::move(pixelOps_))
{
}


// class StretchScalerOutputFactory

template<typename Pixel>
unique_ptr<ScalerOutput<Pixel>> StretchScalerOutputFactory<Pixel>::create(
	SDLOutputSurface& output,
	PixelOperations<Pixel> pixelOps,
	unsigned inWidth)
{
	auto direct = std::make_unique<DirectScalerOutput<Pixel>>(output);
	switch (inWidth) {
	case 320:
		return direct;
	case 288:
		return std::make_unique<StretchScalerOutput288<Pixel>>(
			std::move(direct), std::move(pixelOps));
	case 280:
		return std::make_unique<StretchScalerOutput280<Pixel>>(
			std::move(direct), std::move(pixelOps));
	case 272:
		return std::make_unique<StretchScalerOutput272<Pixel>>(
			std::move(direct), std::move(pixelOps));
	case 256:
		return std::make_unique<StretchScalerOutput256<Pixel>>(
			std::move(direct), std::move(pixelOps));
	default:
		return std::make_unique<StretchScalerOutput<Pixel>>(
			std::move(direct), std::move(pixelOps), inWidth);
	}
}

// Force template instantiation.
#if HAVE_16BPP
template struct StretchScalerOutputFactory<uint16_t>;
#endif
#if HAVE_32BPP
template struct StretchScalerOutputFactory<uint32_t>;
#endif

} // namespace openmsx
