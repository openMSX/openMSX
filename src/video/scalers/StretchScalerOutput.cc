#include "StretchScalerOutput.hh"
#include "DirectScalerOutput.hh"
#include "LineScalers.hh"
#include "PixelOperations.hh"
#include "MemoryOps.hh"
#include "build-info.hh"
#include <cstdint>
#include <memory>
#include <vector>

namespace openmsx {

template<std::unsigned_integral Pixel>
class StretchScalerOutputBase : public ScalerOutput<Pixel>
{
public:
	StretchScalerOutputBase(SDLOutputSurface& out,
	                        PixelOperations<Pixel> pixelOps);
	~StretchScalerOutputBase() override;
	StretchScalerOutputBase(const StretchScalerOutputBase&) = delete;
	StretchScalerOutputBase& operator=(const StretchScalerOutputBase&) = delete;

	[[nodiscard]] unsigned getWidth()  const final;
	[[nodiscard]] unsigned getHeight() const final;
	[[nodiscard]] std::span<Pixel> acquireLine(unsigned y) final;
	void fillLine(unsigned y, Pixel color) override;

protected:
	[[nodiscard]] std::span<Pixel> releasePre(unsigned y, std::span<Pixel> buf);
	void releasePost(unsigned y, std::span<Pixel> dstLine);

	const PixelOperations<Pixel> pixelOps;

private:
	DirectScalerOutput<Pixel> output;
	std::vector<Pixel*> pool;
};

template<std::unsigned_integral Pixel>
class StretchScalerOutput final : public StretchScalerOutputBase<Pixel>
{
public:
	StretchScalerOutput(SDLOutputSurface& out,
	                    PixelOperations<Pixel> pixelOps,
	                    unsigned inWidth);
	void releaseLine(unsigned y, std::span<Pixel> buf) override;

private:
	unsigned inWidth;
};

template<std::unsigned_integral Pixel, unsigned IN_WIDTH, typename SCALE>
class StretchScalerOutputN : public StretchScalerOutputBase<Pixel>
{
public:
	StretchScalerOutputN(SDLOutputSurface& out,
	                     PixelOperations<Pixel> pixelOps);
	void releaseLine(unsigned y, std::span<Pixel> buf) override;
};

template<std::unsigned_integral Pixel>
class StretchScalerOutput256 final
	: public StretchScalerOutputN<Pixel, 256, Scale_4on5<Pixel>>
{
public:
	StretchScalerOutput256(SDLOutputSurface& out,
	                       PixelOperations<Pixel> pixelOps);
};

template<std::unsigned_integral Pixel>
class StretchScalerOutput272 final
	: public StretchScalerOutputN<Pixel, 272, Scale_17on20<Pixel>>
{
public:
	StretchScalerOutput272(SDLOutputSurface& out,
	                       PixelOperations<Pixel> pixelOps);
};

template<std::unsigned_integral Pixel>
class StretchScalerOutput280 final
	: public StretchScalerOutputN<Pixel, 280, Scale_7on8<Pixel>>
{
public:
	StretchScalerOutput280(SDLOutputSurface& out,
	                       PixelOperations<Pixel> pixelOps);
};

template<std::unsigned_integral Pixel>
class StretchScalerOutput288 final
	: public StretchScalerOutputN<Pixel, 288, Scale_9on10<Pixel>>
{
public:
	StretchScalerOutput288(SDLOutputSurface& out,
	                       PixelOperations<Pixel> pixelOps);
};


// class StretchScalerOutputBase

template<std::unsigned_integral Pixel>
StretchScalerOutputBase<Pixel>::StretchScalerOutputBase(
		SDLOutputSurface& out,
		PixelOperations<Pixel> pixelOps_)
	: pixelOps(std::move(pixelOps_))
	, output(out)
{
}

template<std::unsigned_integral Pixel>
StretchScalerOutputBase<Pixel>::~StretchScalerOutputBase()
{
	for (auto& p : pool) {
		MemoryOps::freeAligned(p);
	}
}

template<std::unsigned_integral Pixel>
unsigned StretchScalerOutputBase<Pixel>::getWidth() const
{
	return output.getWidth();
}

template<std::unsigned_integral Pixel>
unsigned StretchScalerOutputBase<Pixel>::getHeight() const
{
	return output.getHeight();
}

template<std::unsigned_integral Pixel>
std::span<Pixel> StretchScalerOutputBase<Pixel>::acquireLine(unsigned /*y*/)
{
	auto width = getWidth();
	if (!pool.empty()) {
		Pixel* buf = pool.back();
		pool.pop_back();
		return {buf, width};
	} else {
		unsigned size = sizeof(Pixel) * width;
		return {static_cast<Pixel*>(MemoryOps::mallocAligned(64, size)),
		        width};
	}
}

template<std::unsigned_integral Pixel>
std::span<Pixel> StretchScalerOutputBase<Pixel>::releasePre(unsigned y, std::span<Pixel> buf)
{
	assert(buf.size() == getWidth());
	pool.push_back(buf.data());
	return output.acquireLine(y);
}

template<std::unsigned_integral Pixel>
void StretchScalerOutputBase<Pixel>::releasePost(unsigned y, std::span<Pixel> dstLine)
{
	output.releaseLine(y, dstLine);
}

template<std::unsigned_integral Pixel>
void StretchScalerOutputBase<Pixel>::fillLine(unsigned y, Pixel color)
{
	MemoryOps::MemSet<Pixel> memset;
	auto dstLine = output.acquireLine(y);
	memset(dstLine, color);
	output.releaseLine(y, dstLine);
}


// class StretchScalerOutput

template<std::unsigned_integral Pixel>
StretchScalerOutput<Pixel>::StretchScalerOutput(
		SDLOutputSurface& out,
		PixelOperations<Pixel> pixelOps_,
		unsigned inWidth_)
	: StretchScalerOutputBase<Pixel>(out, std::move(pixelOps_))
	, inWidth(inWidth_)
{
}

template<std::unsigned_integral Pixel>
void StretchScalerOutput<Pixel>::releaseLine(unsigned y, std::span<Pixel> buf)
{
	auto dstLine = this->releasePre(y, buf);
	auto dstWidth = dstLine.size();

	auto srcWidth = (dstWidth / 320) * inWidth;
	auto srcOffset = (dstWidth - srcWidth) / 2;
	ZoomLine<Pixel> zoom(this->pixelOps);
	zoom(buf.subspan(srcOffset, srcWidth), dstLine);

	this->releasePost(y, dstLine);
}


// class StretchScalerOutputN

template<std::unsigned_integral Pixel, unsigned IN_WIDTH, typename SCALE>
StretchScalerOutputN<Pixel, IN_WIDTH, SCALE>::StretchScalerOutputN(
		SDLOutputSurface& out,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputBase<Pixel>(out, std::move(pixelOps_))
{
}

template<std::unsigned_integral Pixel, unsigned IN_WIDTH, typename SCALE>
void StretchScalerOutputN<Pixel, IN_WIDTH, SCALE>::releaseLine(unsigned y, std::span<Pixel> buf)
{
	auto dstLine = this->releasePre(y, buf);
	auto dstWidth = dstLine.size();

	auto srcWidth = (dstWidth / 320) * IN_WIDTH;
	auto srcOffset = (dstWidth - srcWidth) / 2;
	SCALE scale(this->pixelOps);
	scale(buf.subspan(srcOffset, srcWidth), dstLine);

	this->releasePost(y, dstLine);
}


// class StretchScalerOutput256

template<std::unsigned_integral Pixel>
StretchScalerOutput256<Pixel>::StretchScalerOutput256(
		SDLOutputSurface& out,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputN<Pixel, 256, Scale_4on5<Pixel>>(
		out, std::move(pixelOps_))
{
}


// class StretchScalerOutput272

template<std::unsigned_integral Pixel>
StretchScalerOutput272<Pixel>::StretchScalerOutput272(
		SDLOutputSurface& out,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputN<Pixel, 272, Scale_17on20<Pixel>>(
		out, std::move(pixelOps_))
{
}


// class StretchScalerOutput280

template<std::unsigned_integral Pixel>
StretchScalerOutput280<Pixel>::StretchScalerOutput280(
		SDLOutputSurface& out,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputN<Pixel, 280, Scale_7on8<Pixel>>(
		out, std::move(pixelOps_))
{
}


// class StretchScalerOutput288

template<std::unsigned_integral Pixel>
StretchScalerOutput288<Pixel>::StretchScalerOutput288(
		SDLOutputSurface& out,
		PixelOperations<Pixel> pixelOps_)
	: StretchScalerOutputN<Pixel, 288, Scale_9on10<Pixel>>(
		out, std::move(pixelOps_))
{
}


// class StretchScalerOutputFactory

template<std::unsigned_integral Pixel>
std::unique_ptr<ScalerOutput<Pixel>> StretchScalerOutputFactory<Pixel>::create(
	SDLOutputSurface& output,
	PixelOperations<Pixel> pixelOps,
	unsigned inWidth)
{
	switch (inWidth) {
	case 320:
		return std::make_unique<DirectScalerOutput<Pixel>>(output);
	case 288:
		return std::make_unique<StretchScalerOutput288<Pixel>>(
			output, std::move(pixelOps));
	case 280:
		return std::make_unique<StretchScalerOutput280<Pixel>>(
			output, std::move(pixelOps));
	case 272:
		return std::make_unique<StretchScalerOutput272<Pixel>>(
			output, std::move(pixelOps));
	case 256:
		return std::make_unique<StretchScalerOutput256<Pixel>>(
			output, std::move(pixelOps));
	default:
		return std::make_unique<StretchScalerOutput<Pixel>>(
			output, std::move(pixelOps), inWidth);
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
