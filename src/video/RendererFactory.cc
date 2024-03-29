#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "Reactor.hh"
#include "Display.hh"
#include "unreachable.hh"
#include <memory>

// Video systems:
#include "components.hh"
#include "DummyVideoSystem.hh"
#include "SDLVideoSystem.hh"

// Renderers:
#include "DummyRenderer.hh"
#include "PixelRenderer.hh"
#include "V9990DummyRenderer.hh"
#include "V9990PixelRenderer.hh"

#if COMPONENT_LASERDISC
#include "LDDummyRenderer.hh"
#include "LDPixelRenderer.hh"
#endif

namespace openmsx::RendererFactory {

std::unique_ptr<VideoSystem> createVideoSystem(Reactor& reactor)
{
	Display& display = reactor.getDisplay();
	switch (display.getRenderSettings().getRenderer()) {
		case RenderSettings::DUMMY:
			return std::make_unique<DummyVideoSystem>();
		case RenderSettings::SDLGL_PP:
			return std::make_unique<SDLVideoSystem>(reactor);
		default:
			UNREACHABLE;
	}
}

std::unique_ptr<Renderer> createRenderer(VDP& vdp, Display& display)
{
	switch (display.getRenderSettings().getRenderer()) {
		case RenderSettings::DUMMY:
			return std::make_unique<DummyRenderer>();
		case RenderSettings::SDLGL_PP:
			return std::make_unique<PixelRenderer>(vdp, display);
		default:
			UNREACHABLE;
	}
}

std::unique_ptr<V9990Renderer> createV9990Renderer(V9990& vdp, Display& display)
{
	switch (display.getRenderSettings().getRenderer()) {
		case RenderSettings::DUMMY:
			return std::make_unique<V9990DummyRenderer>();
		case RenderSettings::SDLGL_PP:
			return std::make_unique<V9990PixelRenderer>(vdp);
		default:
			UNREACHABLE;
	}
}

#if COMPONENT_LASERDISC
std::unique_ptr<LDRenderer> createLDRenderer(LaserdiscPlayer& ld, Display& display)
{
	switch (display.getRenderSettings().getRenderer()) {
		case RenderSettings::DUMMY:
			return std::make_unique<LDDummyRenderer>();
		case RenderSettings::SDLGL_PP:
			return std::make_unique<LDPixelRenderer>(ld, display);
		default:
			UNREACHABLE;
	}
}
#endif

} // namespace openmsx::RendererFactory
