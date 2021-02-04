#ifndef MOUSE_HH
#define MOUSE_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;

class Mouse final : public JoystickDevice, private MSXEventListener
                  , private StateChangeListener
{
public:
	Mouse(MSXEventDistributor& eventDistributor,
	      StateChangeDistributor& stateChangeDistributor);
	~Mouse() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	// MSXEventListener
	void signalMSXEvent(const std::shared_ptr<const Event>& event,
	                    EmuTime::param time) noexcept override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

	void createMouseStateChange(EmuTime::param time,
		int deltaX, int deltaY, byte press, byte release);
	void emulateJoystick();
	void plugHelper2();

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	EmuTime lastTime;
	int phase;
	int xrel, yrel;         // latched X/Y values, these are returned to the MSX
	int curxrel, curyrel;   // running X/Y values, already scaled down
	int absHostX, absHostY; // running X/Y values, not yet scaled down
	byte status;
	bool mouseMode;
};
SERIALIZE_CLASS_VERSION(Mouse, 4);

} // namespace openmsx

#endif
