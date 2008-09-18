// $Id$

#ifndef KEYJOYSTICK_HH
#define KEYJOYSTICK_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class CommandController;
class KeyCodeSetting;
class MSXEventDistributor;

class KeyJoystick : public JoystickDevice, private MSXEventListener
{
public:
	KeyJoystick(CommandController& commandController,
	            MSXEventDistributor& eventDistributor,
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	void allUp();

	MSXEventDistributor& eventDistributor;
	const std::string name;
	const std::auto_ptr<KeyCodeSetting> up;
	const std::auto_ptr<KeyCodeSetting> down;
	const std::auto_ptr<KeyCodeSetting> left;
	const std::auto_ptr<KeyCodeSetting> right;
	const std::auto_ptr<KeyCodeSetting> trigA;
	const std::auto_ptr<KeyCodeSetting> trigB;

	byte status;
};

} // namespace openmsx

#endif
