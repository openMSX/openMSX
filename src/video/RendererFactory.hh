// $Id$

#ifndef RENDERERFACTORY_HH
#define RENDERERFACTORY_HH

#include <memory>

namespace openmsx {

class Reactor;
class CommandController;
class Display;
class VideoSystem;
class Renderer;
class VDP;
class V9990Renderer;
class V9990;
template <typename T> class EnumSetting;

/** Interface for renderer factories.
  * Every Renderer type has its own RendererFactory.
  * A RendererFactory can be queried about the availability of the
  * associated Renderer and can instantiate that Renderer.
  */
class RendererFactory
{
public:
	/** Enumeration of Renderers known to openMSX.
	  * This is the full list, the list of available renderers may be smaller.
	  */
	enum RendererID { UNINITIALIZED, DUMMY, SDL,
	                  SDLGL, SDLGL2, SDLGL_PP, SDLGL_FB16, SDLGL_FB32 };

	virtual ~RendererFactory() {}

	typedef EnumSetting<RendererID> RendererSetting;

	/** Create the video system required by the current renderer setting.
	  */
	static VideoSystem* createVideoSystem(Reactor& reactor);

	/** Create the Renderer selected by the current renderer setting.
	  * @param vdp The VDP whose display will be rendered.
	  */
	static Renderer* createRenderer(VDP& vdp, Display& display);

	/** Create the V9990 Renderer selected by the current renderer setting.
	  * @param vdp The V9990 VDP whose display will be rendered.
	  */
	static V9990Renderer* createV9990Renderer(V9990& vdp, Display& display);

	/** Create the renderer setting.
	  * The map of this setting contains only the available renderers.
	  */
	static std::auto_ptr<RendererSetting> createRendererSetting(
			CommandController& commandController);
};

} // namespace openmsx

#endif
