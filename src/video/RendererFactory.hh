// $Id$

#ifndef __RENDERERFACTORY_HH__
#define __RENDERERFACTORY_HH__

#include "EnumSetting.hh"
#include "GLUtil.hh" // for __OPENGL_AVAILABLE__
#include "probed_defs.hh" // for HAVE_X11
#include <SDL.h>
#include <string>


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
	static RendererSetting* createRendererSetting(
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
	virtual Renderer* create(VDP* vdp) = 0;

protected:
	virtual ~RendererFactory() {}

private:
	/** Get the factory selected by the current renderer setting.
	  * @return The RendererFactory that can create the renderer.
	  */
	static RendererFactory* getCurrent();
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

	bool isAvailable();

	Renderer* create(VDP* vdp);
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

	bool isAvailable();

	Renderer* create(VDP* vdp);
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

	bool isAvailable();

	Renderer* create(VDP* vdp);
};

#ifdef __OPENGL_AVAILABLE__

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

	bool isAvailable();

	Renderer* create(VDP* vdp);
};

#endif // __OPENGL_AVAILABLE__

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

	Renderer* create(VDP* vdp);
};

#endif // HAVE_X11

} // namespace openmsx

#endif //__RENDERERFACTORY_HH__

