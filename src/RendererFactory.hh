// $Id$

#ifndef __RENDERERFACTORY_HH__
#define __RENDERERFACTORY_HH__

#include <string>

// For __SDLGLRENDERER_AVAILABLE__
#include "SDLGLRenderer.hh"

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

	/** Gets the name of the associated renderer.
	  */
	//virtual const std::string getName() = 0;

	/** Is the associated Renderer available?
	  * Availability may depend on the presence of libraries, the graphics
	  * hardware or the graphics system that is currently running.
	  * This method should return the same value every time it is called.
	  */
	virtual bool isAvailable() = 0;

	/** Instantiate the associated Renderer.
	  * @param vdp VDP whose state will be rendered.
	  * @param time Moment in emulated time the renderer is created.
	  * @return a newly created Renderer, or NULL if creation failed.
	  *   TODO: Throwing an exception would be cleaner.
	  */
	virtual Renderer *create(VDP *vdp, const EmuTime &time) = 0;

};

/** RendererFactory for SDLHiRenderer.
  */
class SDLHiRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	SDLHiRendererFactory() { }

	const std::string getName() {
		return "SDLHi";
	}

	bool isAvailable();

	Renderer *create(VDP *vdp, const EmuTime &time);

};

/** RendererFactory for SDLLoRenderer.
  */
class SDLLoRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	SDLLoRendererFactory() { }

	const std::string getName() {
		return "SDLLo";
	}

	bool isAvailable();

	Renderer *create(VDP *vdp, const EmuTime &time);

};

#ifdef __SDLGLRENDERER_AVAILABLE__

/** RendererFactory for SDLGLRenderer.
  */
class SDLGLRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	SDLGLRendererFactory() { }

	const std::string getName() {
		return "SDLGL";
	}

	bool isAvailable();

	Renderer *create(VDP *vdp, const EmuTime &time);

};

#endif // __SDLGLRENDERER_AVAILABLE__

/** RendererFactory for XRenderer.
  */
class XRendererFactory: public RendererFactory
{
public:

	/** TODO: Convert to singleton?
	  */
	XRendererFactory() { }

	const std::string getName() {
		return "Xlib";
	}

	bool isAvailable();

	Renderer *create(VDP *vdp, const EmuTime &time);

};

#endif //__RENDERERFACTORY_HH__

