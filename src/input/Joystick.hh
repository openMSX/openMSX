// $Id$

#ifndef __JOYSTICK_HH__
#define __JOYSTICK_HH__

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "MSXException.hh"
#include <SDL/SDL.h>

namespace openmsx {

class PluggingController;

/** Uses an SDL joystick to emulate an MSX joystick.
  */
class Joystick : public JoystickDevice, EventListener
{
public:
	/** Register all available SDL joysticks.
	  * @param controller Register here.
	  */
	static void registerAll(PluggingController* controller);

	//Pluggable
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void plug(Connector* connector, const EmuTime& time)
		throw(PlugException);
	virtual void unplug(const EmuTime& time);

	//JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	//EventListener
	virtual bool signalEvent(const SDL_Event& event) throw();

private:
	Joystick(int joyNum);
	virtual ~Joystick();

	static const int THRESHOLD = 32768/10;

	string name;
	string desc;

	int joyNum;
	SDL_Joystick* joystick;
	byte status;
};

} // namespace openmsx
#endif
