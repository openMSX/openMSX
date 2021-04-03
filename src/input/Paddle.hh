#ifndef PADDLE_HH
#define PADDLE_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;

class Paddle final : public JoystickDevice, private MSXEventListener
                   , private StateChangeListener
{
public:
	Paddle(MSXEventDistributor& eventDistributor,
	       StateChangeDistributor& stateChangeDistributor);
	~Paddle() override;

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
	void signalMSXEvent(const Event& event,
	                    EmuTime::param time) noexcept override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	EmuTime lastPulse;
	byte analogValue;
	byte lastInput;
};

} // namespace openmsx

#endif
