#ifndef PADDLE_HH
#define PADDLE_HH

#include "EmuTime.hh"
#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include <cstdint>

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
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime time) noexcept override;

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	EmuTime lastPulse = EmuTime::zero();
	uint8_t analogValue = 128;
	uint8_t lastInput = 0;
};

} // namespace openmsx

#endif
