#ifndef KEYJOYSTICK_HH
#define KEYJOYSTICK_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class CommandController;
class KeyCodeSetting;
class MSXEventDistributor;
class StateChangeDistributor;

class KeyJoystick final : public JoystickDevice, private MSXEventListener
                        , private StateChangeListener
{
public:
	KeyJoystick(CommandController& commandController,
	            MSXEventDistributor& eventDistributor,
	            StateChangeDistributor& stateChangeDistributor,
	            const std::string& name);
	~KeyJoystick();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Pluggable
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	// KeyJoystickDevice
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);

	// MSXEventListener
	virtual void signalEvent(const std::shared_ptr<const Event>& event,
	                         EmuTime::param time);
	// StateChangeListener
	virtual void signalStateChange(const std::shared_ptr<StateChange>& event);
	virtual void stopReplay(EmuTime::param time);

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	const std::string name;
	const std::unique_ptr<KeyCodeSetting> up;
	const std::unique_ptr<KeyCodeSetting> down;
	const std::unique_ptr<KeyCodeSetting> left;
	const std::unique_ptr<KeyCodeSetting> right;
	const std::unique_ptr<KeyCodeSetting> trigA;
	const std::unique_ptr<KeyCodeSetting> trigB;

	byte status;
	bool pin8;
};
SERIALIZE_CLASS_VERSION(KeyJoystick, 2);

} // namespace openmsx

#endif
