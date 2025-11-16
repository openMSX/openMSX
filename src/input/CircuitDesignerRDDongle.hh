#ifndef CIRCUITDESIGNERRDDONGLE_HH
#define CIRCUITDESIGNERRDDONGLE_HH

#include "JoystickDevice.hh"

namespace openmsx {

class CircuitDesignerRDDongle final : public JoystickDevice
{
public:
	// Pluggable
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	uint8_t status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                 JOY_BUTTONA | JOY_BUTTONB;
};

} // namespace openmsx

#endif
