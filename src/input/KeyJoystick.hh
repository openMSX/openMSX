// $Id$

#ifndef KEYJOYSTICK_HH
#define KEYJOYSTICK_HH

#include "JoystickDevice.hh"
#include "UserInputEventListener.hh"
#include <memory>

namespace openmsx {

class CommandController;
class KeyCodeSetting;
class UserInputEventDistributor;

class KeyJoystick : public JoystickDevice, private UserInputEventListener
{
public:
	KeyJoystick(CommandController& commandController,
	            UserInputEventDistributor& eventDistributor,
	            const std::string& name);
	virtual ~KeyJoystick();

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// KeyJoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

private:
	// UserInputEventListener
	virtual void signalEvent(const Event& event);

	void allUp();

	std::auto_ptr<KeyCodeSetting> up;
	std::auto_ptr<KeyCodeSetting> right;
	std::auto_ptr<KeyCodeSetting> down;
	std::auto_ptr<KeyCodeSetting> left;
	std::auto_ptr<KeyCodeSetting> trigA;
	std::auto_ptr<KeyCodeSetting> trigB;

	UserInputEventDistributor& eventDistributor;
	std::string name;
	byte status;
};

} // namespace openmsx

#endif
