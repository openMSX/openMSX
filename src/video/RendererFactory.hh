#ifndef RENDERERFACTORY_HH
#define RENDERERFACTORY_HH

#include <memory>
#include "components.hh"

namespace openmsx {

class Reactor;
class Display;
class VideoSystem;
class Renderer;
class VDP;
class V9990Renderer;
class V9990;
class LDRenderer;
class LaserdiscPlayer;

/** Interface for renderer factories.
  * Every Renderer type has its own RendererFactory.
  * A RendererFactory can be queried about the availability of the
  * associated Renderer and can instantiate that Renderer.
  */
namespace RendererFactory
{
	/** Create the video system required by the current renderer setting.
	  */
	[[nodiscard]] std::unique_ptr<VideoSystem> createVideoSystem(Reactor& reactor);

	/** Create the Renderer selected by the current renderer setting.
	  * @param vdp The VDP whose display will be rendered.
	  * @param display TODO
	  */
	[[nodiscard]] std::unique_ptr<Renderer> createRenderer(VDP& vdp, Display& display);

	/** Create the V9990 Renderer selected by the current renderer setting.
	  * @param vdp The V9990 VDP whose display will be rendered.
	  * @param display TODO
	  */
	[[nodiscard]] std::unique_ptr<V9990Renderer> createV9990Renderer(
		V9990& vdp, Display& display);

#if COMPONENT_LASERDISC
	/** Create the Laserdisc Renderer
	  * @param ld The Laserdisc player whose display will be rendered.
	  * @param display TODO
	  */
	[[nodiscard]] std::unique_ptr<LDRenderer> createLDRenderer(
		LaserdiscPlayer& ld, Display& display);
#endif

} // namespace RendererFactory
} // namespace openmsx

#endif
