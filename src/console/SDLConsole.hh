// $Id$

#ifndef SDLCONSOLE_HH
#define SDLCONSOLE_HH

#include "OSDConsoleRenderer.hh"
#include <memory>

class SDL_Surface;

namespace openmsx {

class SDLImage;

class SDLConsole : public OSDConsoleRenderer
{
public:
	SDLConsole(MSXMotherBoard& motherBoard, SDL_Surface* screen);

	virtual void loadFont(const std::string& filename);
	virtual void loadBackground(const std::string& filename);
	virtual unsigned getScreenW() const;
	virtual unsigned getScreenH() const;

	virtual void paint();
	virtual const std::string& getName();

private:
	void updateConsoleRect();

	SDL_Surface* outputScreen;
	std::auto_ptr<SDLImage> backgroundImage;
	std::string backgroundName;
};

} // namespace openmsx

#endif
