// $Id$

#ifndef __RENDERERFACTORY_HH__
#define __RENDERERFACTORY_HH__

#include <memory>
#include "components.hh"
#include "probed_defs.hh" // for HAVE_X11 (should be component instead?)

namespace openmsx {

class VideoSystem;
class Renderer;
class VDP;
class V9990Renderer;
class V9990;
class XMLElement;
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
	enum RendererID { DUMMY, SDLHI, SDLLO, SDLGL, XLIB };

	virtual ~RendererFactory() {}

	typedef EnumSetting<RendererID> RendererSetting;

	/** Create the video system required by the current renderer setting.
	  */
	static VideoSystem* createVideoSystem();

	/** Create the Renderer selected by the current renderer setting.
	  * @param vdp The VDP whose display will be rendered.
	  */
	static Renderer* createRenderer(VDP* vdp);

	/** Create the V9990 Renderer selected by the current renderer setting.
	  * @param vdp The V9990 VDP whose display will be rendered.
	  */
	static V9990Renderer* createV9990Renderer(V9990* vdp);

	/** Create the renderer setting.
	  * The map of this setting contains only the available renderers.
	  */
	static std::auto_ptr<RendererSetting> createRendererSetting();
};

} // namespace openmsx

#endif //__RENDERERFACTORY_HH__

