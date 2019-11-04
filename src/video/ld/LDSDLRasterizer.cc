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

template <class Pixel>
LDSDLRasterizer<Pixel>::LDSDLRasterizer(
		OutputSurface& screen,
		std::unique_ptr<PostProcessor> postProcessor_)
	: postProcessor(std::move(postProcessor_))
	, workFrame(std::make_unique<RawFrame>(screen.getPixelFormat(), 640, 480))
{
}

template <class Pixel>
LDSDLRasterizer<Pixel>::~LDSDLRasterizer() = default;

template <class Pixel>
PostProcessor* LDSDLRasterizer<Pixel>::getPostProcessor() const
{
	return postProcessor.get();
}

template <class Pixel>
void LDSDLRasterizer<Pixel>::frameStart(EmuTime::param time)
{
	workFrame = postProcessor->rotateFrames(std::move(workFrame), time);
}

template<class Pixel>
void LDSDLRasterizer<Pixel>::drawBlank(int r, int g, int b)
{
	// We should really be presenting the "LASERVISION" text
	// here, like the real laserdisc player does. Note that this
	// changes when seeking or starting to play.
	auto background = static_cast<Pixel>(workFrame->getPixelFormat().map(r, g, b));
	for (int y = 0; y < 480; ++y) {
		workFrame->setBlank(y, background);
	}
}

template<class Pixel>
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
