#include "LDSDLRasterizer.hh"
#include "RawFrame.hh"
#include "PostProcessor.hh"
#include "OutputSurface.hh"
#include "PixelFormat.hh"
#include "build-info.hh"
#include "components.hh"
#include <cstdint>
#include <memory>

namespace openmsx {

template<typename Pixel>
LDSDLRasterizer<Pixel>::LDSDLRasterizer(
		OutputSurface& screen,
		std::unique_ptr<PostProcessor> postProcessor_)
	: postProcessor(std::move(postProcessor_))
	, workFrame(std::make_unique<RawFrame>(screen.getPixelFormat(), 640, 480))
{
}

template<typename Pixel>
LDSDLRasterizer<Pixel>::~LDSDLRasterizer() = default;

template<typename Pixel>
PostProcessor* LDSDLRasterizer<Pixel>::getPostProcessor() const
{
	return postProcessor.get();
}

template<typename Pixel>
void LDSDLRasterizer<Pixel>::frameStart(EmuTime::param time)
{
	workFrame = postProcessor->rotateFrames(std::move(workFrame), time);
}

template<typename Pixel>
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

template<typename Pixel>
RawFrame* LDSDLRasterizer<Pixel>::getRawFrame()
{
	return workFrame.get();
}


// Force template instantiation.
#if HAVE_16BPP
template class LDSDLRasterizer<uint16_t>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class LDSDLRasterizer<uint32_t>;
#endif

} // namespace openmsx
