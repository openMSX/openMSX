// $Id$

#ifndef KEYJOYSTICK_HH
#define KEYJOYSTICK_HH

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "XMLElementListener.hh"
#include "Keys.hh"

namespace openmsx {

class KeyJoystick : public JoystickDevice, private EventListener,
                    private XMLElementListener
{
public:
	KeyJoystick();
	virtual ~KeyJoystick();

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// KeyJoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

private:
	// EventListener
	virtual bool signalEvent(const Event& event);

	// XMLElementListener
	virtual void updateData(const XMLElement& element);
	virtual void childAdded(const XMLElement& parent,
	                        const XMLElement& child);
	
	void readKeys();
	void allUp();

	byte status;

	Keys::KeyCode upKey;
	Keys::KeyCode rightKey;
	Keys::KeyCode downKey;
	Keys::KeyCode leftKey;
	Keys::KeyCode buttonAKey;
	Keys::KeyCode buttonBKey;
	XMLElement& keyJoyConfig;
};

} // namespace openmsx

#endif
