// $Id$

#ifndef SDLCONSOLE_HH
#define SDLCONSOLE_HH

#include "OSDConsoleRenderer.hh"
#include <memory>


namespace openmsx {

class OutputSurface;
class SDLImage;

class SDLConsole : public OSDConsoleRenderer
{
public:
	SDLConsole(Reactor& reactor, OutputSurface& output);

	virtual void loadFont(const std::string& filename);
	virtual void loadBackground(const std::string& filename);
	virtual unsigned getScreenW() const;
	virtual unsigned getScreenH() const;

	virtual void paint();
	virtual const std::string& getName();

private:
	void updateConsoleRect();

	OutputSurface& output;
	std::auto_ptr<SDLImage> backgroundImage;
	std::string backgroundName;
};

} // namespace openmsx

#endif
