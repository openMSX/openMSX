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
	const std::string& getName() const override;
	string_ref getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// KeyJoystickDevice
	byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	// MSXEventListener
	void signalEvent(const std::shared_ptr<const Event>& event,
	                 EmuTime::param time) override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) override;

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
