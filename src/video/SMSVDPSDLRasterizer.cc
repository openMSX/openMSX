#include "SMSVDPSDLRasterizer.hh"

#include "SMSVDP.hh"

#include "Display.hh"
#include "OutputSurface.hh"
#include "PostProcessor.hh"
#include "RawFrame.hh"
#include "RenderSettings.hh"

#include "one_of.hh"
#include "xrange.hh"

#include <cassert>
#include <ranges>

using namespace gl;

namespace openmsx {

SMSVDPSDLRasterizer::SMSVDPSDLRasterizer(
		SMSVDP& vdp_, Display& display, OutputSurface& screen_,
		std::unique_ptr<PostProcessor> postProcessor_)
	: vdp(vdp_)
	, screen(screen_)
	, renderSettings(display.getRenderSettings())
	, workFrame(std::make_unique<RawFrame>(320, 240))
	, postProcessor(std::move(postProcessor_))
{
	precalcPalette();

	renderSettings.getGammaSetting()      .attach(*this);
	renderSettings.getBrightnessSetting() .attach(*this);
	renderSettings.getContrastSetting()   .attach(*this);
	renderSettings.getColorMatrixSetting().attach(*this);
}

SMSVDPSDLRasterizer::~SMSVDPSDLRasterizer()
{
	renderSettings.getColorMatrixSetting().detach(*this);
	renderSettings.getGammaSetting()      .detach(*this);
	renderSettings.getBrightnessSetting() .detach(*this);
	renderSettings.getContrastSetting()   .detach(*this);
}

PostProcessor* SMSVDPSDLRasterizer::getPostProcessor() const
{
	return postProcessor.get();
}

void SMSVDPSDLRasterizer::precalcPalette()
{
	for (auto [rgb, out] : std::views::zip(vdp.getPaletteColors(), palette)) {
		out = screen.mapRGB(renderSettings.transformRGB(
			vec3((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF) * (1.0f / 255.0f)));
	}
}

void SMSVDPSDLRasterizer::frameEnd(EmuTime time)
{
	workFrame = postProcessor->rotateFrames(std::move(workFrame), time);
	workFrame->init(RawFrame::FieldType::NONINTERLACED);
}

void SMSVDPSDLRasterizer::drawLine(
	unsigned y, std::span<const uint8_t, 320> paletteIndices)
{
	assert(y < 240);
	auto line = workFrame->getLineDirect(y);
	for (auto i : xrange(320)) {
		line[i] = palette[paletteIndices[i]];
	}
	workFrame->setLineWidth(y, 320);
}

void SMSVDPSDLRasterizer::update(const Setting& setting) noexcept
{
	if (&setting == one_of(&renderSettings.getGammaSetting(),
	                       &renderSettings.getBrightnessSetting(),
	                       &renderSettings.getContrastSetting(),
	                       &renderSettings.getColorMatrixSetting())) {
		precalcPalette();
	}
}

} // namespace openmsx
