// $Id$

#ifndef __MOUSE_HH__
#define __MOUSE_HH__

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "EmuTime.hh"


class Mouse : public JoystickDevice, EventListener
{
	public:
		Mouse(const EmuTime &time);
		virtual ~Mouse();
		
		//Pluggable
		virtual const std::string &getName() const;
		
		//JoystickDevice
		virtual byte read(const EmuTime &time);
		virtual void write(byte value, const EmuTime &time);
	
		//EventListener
		virtual bool signalEvent(SDL_Event &event);

	private:
		static const int SCALE = 2;
		static const int FAZE_XHIGH = 0;
		static const int FAZE_XLOW  = 1;
		static const int FAZE_YHIGH = 2;
		static const int FAZE_YLOW  = 3;
		static const int STROBE = 0x04;

		byte status;
		int faze;
		int xrel, yrel;
		int curxrel, curyrel;
		EmuTimeFreq<1000> lastTime;	// ms
};
#endif
