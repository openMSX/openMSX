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
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;
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
	int shiftReg = 0; // the 9 bit shift degrades to 0
	int dialPos;
	uint8_t buttonStatus = 0x3E;
	uint8_t lastValue = 0;
};
SERIALIZE_CLASS_VERSION(ArkanoidPad, 2);

} // namespace openmsx

#endif
