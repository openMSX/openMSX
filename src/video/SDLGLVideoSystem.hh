// $Id$

#ifndef __SDLGLVIDEOSYSTEM_HH__
#define __SDLGLVIDEOSYSTEM_HH__

#include "Display.hh"


namespace openmsx {


class SDLGLVideoSystem: public VideoSystem
{
public:
	SDLGLVideoSystem();
	virtual ~SDLGLVideoSystem();

	// VideoSystem interface:
	virtual void flush();
	virtual void takeScreenShot(const string& filename);
};

} // namespace openmsx

#endif // __SDLGLVIDEOSYSTEM_HH__
