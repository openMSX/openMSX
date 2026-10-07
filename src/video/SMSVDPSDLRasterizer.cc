#include "SMSVDPSDLRasterizer.hh"

#include "PostProcessor.hh"
#include "RawFrame.hh"

#include "xrange.hh"

#include <cassert>

namespace openmsx {

SMSVDPSDLRasterizer::SMSVDPSDLRasterizer(
		std::unique_ptr<PostProcessor> postProcessor_)
	: workFrame(std::make_unique<RawFrame>(320, 240))
	, postProcessor(std::move(postProcessor_))
{
}

SMSVDPSDLRasterizer::~SMSVDPSDLRasterizer() = default;

PostProcessor* SMSVDPSDLRasterizer::getPostProcessor() const
{
	return postProcessor.get();
}

void SMSVDPSDLRasterizer::reset()
{
	// nothing
}

void SMSVDPSDLRasterizer::frameStart()
{
	// nothing
}

void SMSVDPSDLRasterizer::frameEnd(EmuTime time)
{
	workFrame = postProcessor->rotateFrames(std::move(workFrame), time);
	workFrame->init(RawFrame::FieldType::NONINTERLACED);
}

void SMSVDPSDLRasterizer::drawLine(
	unsigned y, std::span<const uint32_t, 320> pixels)
{
	assert(y < 240);
	auto line = workFrame->getLineDirect(y);
	// Convert 0x00RRGGBB to openMSX' native pixel format (0xAABBGGRR).
	for (auto i : xrange(320)) {
		const uint32_t p = pixels[i];
		line[i] = ((p >> 16) & 0xFF)        // R
		        | (p & 0x0000FF00)          // G
		        | ((p & 0x000000FF) << 16)  // B
		        | 0xFF000000;               // A
	}
	workFrame->setLineWidth(y, 320);
}

} // namespace openmsx
