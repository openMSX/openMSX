#include "LDSDLRasterizer.hh"
#include "RawFrame.hh"
#include "PostProcessor.hh"
#include "OutputSurface.hh"
#include "PixelFormat.hh"
#include <cstdint>
#include <memory>

namespace openmsx {

template<std::unsigned_integral Pixel>
LDSDLRasterizer<Pixel>::LDSDLRasterizer(
		OutputSurface& screen,
		std::unique_ptr<PostProcessor> postProcessor_)
	: postProcessor(std::move(postProcessor_))
	, workFrame(std::make_unique<RawFrame>(screen.getPixelFormat(), 640, 480))
{
}

template<std::unsigned_integral Pixel>
LDSDLRasterizer<Pixel>::~LDSDLRasterizer() = default;

template<std::unsigned_integral Pixel>
PostProcessor* LDSDLRasterizer<Pixel>::getPostProcessor() const
{
	return postProcessor.get();
}

template<std::unsigned_integral Pixel>
void LDSDLRasterizer<Pixel>::frameStart(EmuTime::param time)
{
	workFrame = postProcessor->rotateFrames(std::move(workFrame), time);
}

template<std::unsigned_integral Pixel>
void LDSDLRasterizer<Pixel>::drawBlank(int r, int g, int b)
{
	// We should really be presenting the "LASERVISION" text
	// here, like the real laserdisc player does. Note that this
	// changes when seeking or starting to play.
	auto background = static_cast<Pixel>(workFrame->getPixelFormat().map(r, g, b));
	for (auto y : xrange(480)) {
		workFrame->setBlank(y, background);
	}
}

template<std::unsigned_integral Pixel>
RawFrame* LDSDLRasterizer<Pixel>::getRawFrame()
{
	return workFrame.get();
}


// Force template instantiation.
template class LDSDLRasterizer<uint32_t>;

} // namespace openmsx
