// $Id$

#ifndef __RENDERERFACTORY_HH__
#define __RENDERERFACTORY_HH__

#include <string>
#include <memory>
#include "EnumSetting.hh"
#include "components.hh"
#include "probed_defs.hh" // for HAVE_X11 (should be component instead?)

using std::string;
using std::auto_ptr;


namespace openmsx {

class Renderer;
class VDP;
class XMLElement;

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

	/** Create the Renderer selected by the current renderer setting.
	  * Use this method for initial renderer creation in the main thread.
	  * @param vdp The VDP whose display will be rendered.
	  */
	static Renderer* createRenderer(VDP* vdp);

	/** Create the renderer setting.
	  * The map of this setting contains only the available renderers.
	  * @param defaultRenderer The name of the default renderer
	  */
	static auto_ptr<RendererSetting> createRendererSetting();
};

} // namespace openmsx

#endif //__RENDERERFACTORY_HH__

