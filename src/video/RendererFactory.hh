// $Id$

#ifndef __RENDERERFACTORY_HH__
#define __RENDERERFACTORY_HH__

#include <string>
#include <memory>
#include <SDL.h>
#include "EnumSetting.hh"
#include "components.hh"
#include "probed_defs.hh" // for HAVE_X11 (should be component instead?)

using std::string;
using std::auto_ptr;

namespace openmsx {

class Renderer;
class EmuTime;
class VDP;

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
	static auto_ptr<Renderer> createRenderer(VDP* vdp);

	/** Create the renderer setting.
	  * The map of this setting contains only the available renderers.
	  * @param defaultRenderer The name of the default renderer
	  */
	static auto_ptr<RendererSetting> createRendererSetting(
		const string& defaultRenderer);

	/** Gets the name of the associated renderer.
	  */
	//virtual const string getName() = 0;

	/** Is the associated Renderer available?
	  * Availability may depend on the presence of libraries, the graphics
	  * hardware or the graphics system that is currently running.
	  * This method should return the same value every time it is called.
	  */
	virtual bool isAvailable() = 0;

	/** Instantiate the associated Renderer.
	  * @param vdp VDP whose state will be rendered.
	  * @return A newly created Renderer, or NULL if creation failed.
	  *   TODO: Throwing an exception would be cleaner.
	  */
	virtual auto_ptr<Renderer> create(VDP* vdp) = 0;

private:
	/** Get the factory selected by the current renderer setting.
	  * @return The RendererFactory that can create the renderer.
	  */
	static auto_ptr<RendererFactory> getCurrent();
};

/** RendererFactory for DummyRenderer.
  */
class DummyRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	DummyRendererFactory() { }

	const string getName() {
		return "Dummy";
	}

	virtual bool isAvailable();

	virtual auto_ptr<Renderer> create(VDP* vdp);
};

/** RendererFactory for SDLHiRenderer.
  */
class SDLHiRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	SDLHiRendererFactory() { }

	const string getName() {
		return "SDLHi";
	}

	virtual bool isAvailable();

	virtual auto_ptr<Renderer> create(VDP* vdp);
};

/** RendererFactory for SDLLoRenderer.
  */
class SDLLoRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	SDLLoRendererFactory() { }

	const string getName() {
		return "SDLLo";
	}

	virtual bool isAvailable();

	virtual auto_ptr<Renderer> create(VDP* vdp);
};

#ifdef COMPONENT_GL

/** RendererFactory for SDLGLRenderer.
  */
class SDLGLRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	SDLGLRendererFactory() { }

	const string getName() {
		return "SDLGL";
	}

	virtual bool isAvailable();

	virtual auto_ptr<Renderer> create(VDP* vdp);
};

#endif // COMPONENT_GL

#ifdef HAVE_X11

/** RendererFactory for XRenderer.
  */
class XRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	XRendererFactory() { }

	const string getName() {
		return "Xlib";
	}

	bool isAvailable();

	auto_ptr<Renderer> create(VDP* vdp);
};

#endif // HAVE_X11

} // namespace openmsx

#endif //__RENDERERFACTORY_HH__

