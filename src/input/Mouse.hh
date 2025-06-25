#ifndef MOUSE_HH
#define MOUSE_HH

#include "EmuTime.hh"
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

	void createMouseStateChange(EmuTime time,
		int deltaX, int deltaY, uint8_t press, uint8_t release);
	void emulateJoystick();
	void plugHelper2();

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	EmuTime lastTime = EmuTime::zero();
	int phase;
	int xRel = 0, yRel = 0;               // latched X/Y values, these are returned to the MSX
	int curXRel = 0, curYRel = 0;         // running X/Y values, already scaled down
	int fractionalX = 0, fractionalY = 0; // running X/Y values, not yet scaled down
	uint8_t status = JOY_BUTTONA | JOY_BUTTONB;
	bool mouseMode = true;
};
SERIALIZE_CLASS_VERSION(Mouse, 4);

} // namespace openmsx

#endif
