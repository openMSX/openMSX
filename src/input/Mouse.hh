// $Id$

#ifndef __MOUSE_HH__
#define __MOUSE_HH__

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "EmuTime.hh"


namespace openmsx {

class Mouse : public JoystickDevice, EventListener
{
	public:
		Mouse();
		virtual ~Mouse();

		//Pluggable
		virtual const string &getName() const;
		virtual void plug(Connector* connector, const EmuTime& time) throw();
		virtual void unplug(const EmuTime& time);

		//JoystickDevice
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);

		//EventListener
		virtual bool signalEvent(SDL_Event &event);

	private:
		void emulateJoystick();
		
		byte status;
		int faze;
		int xrel, yrel;
		int curxrel, curyrel;
		EmuTimeFreq<1000> lastTime;	// ms
		bool mouseMode;
};

} // namespace openmsx
#endif
