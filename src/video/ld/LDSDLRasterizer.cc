#include "LDSDLRasterizer.hh"
#include "RawFrame.hh"
#include "PostProcessor.hh"
#include "PixelOperations.hh"
#include <cstdint>

namespace openmsx {

LDSDLRasterizer::LDSDLRasterizer(
		std::unique_ptr<PostProcessor> postProcessor_)
	: postProcessor(std::move(postProcessor_))
	, workFrame(std::make_unique<RawFrame>(640, 480))
{
}

LDSDLRasterizer::~LDSDLRasterizer() = default;

PostProcessor* LDSDLRasterizer::getPostProcessor() const
{
	return postProcessor.get();
}

void LDSDLRasterizer::frameStart(EmuTime time)
{
	workFrame = postProcessor->rotateFrames(std::move(workFrame), time);
}

void LDSDLRasterizer::drawBlank(int r, int g, int b)
{
	// We should really be presenting the "LASERVISION" text
	// here, like the real laserdisc player does. Note that this
	// changes when seeking or starting to play.
	PixelOperations pixelOps;
	auto background = pixelOps.combine(r, g, b);
	for (auto y : xrange(480)) {
		workFrame->setBlank(y, background);
	}
}

RawFrame* LDSDLRasterizer::getRawFrame()
{
	return workFrame.get();
}

} // namespace openmsx
