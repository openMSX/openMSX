// $Id$

#ifndef __MOUSE_HH__
#define __MOUSE_HH__

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "EmuTime.hh"

namespace openmsx {

class Mouse : public JoystickDevice, private EventListener
{
public:
	Mouse();
	virtual ~Mouse();

	//Pluggable
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time) throw();
	virtual void unplugHelper(const EmuTime& time);

	//JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	//EventListener
	virtual bool signalEvent(const Event& event) throw();

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

#endif // __MOUSE_HH__
