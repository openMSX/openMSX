#ifndef ARKANOIDPAD_HH
#define ARKANOIDPAD_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;

class ArkanoidPad final : public JoystickDevice, private MSXEventListener
                        , private StateChangeListener
{
public:
	explicit ArkanoidPad(MSXEventDistributor& eventDistributor,
	                     StateChangeDistributor& stateChangeDistributor);
	~ArkanoidPad() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime::param time) override;
	void write(uint8_t value, EmuTime::param time) override;

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime::param time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	int shiftReg = 0; // the 9 bit shift degrades to 0
	int dialPos;
	uint8_t buttonStatus = 0x3E;
	uint8_t lastValue = 0;
};
SERIALIZE_CLASS_VERSION(ArkanoidPad, 2);

} // namespace openmsx

#endif
