#ifndef Y8960SYSTEMCONTROLLER_HH
#define Y8960SYSTEMCONTROLLER_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"

namespace openmsx {

/** The System Controller of the Y8960 cartridge, on I/O 40h-4Fh.
  *
  * Writing 80h to 40h enables it, which is the switched I/O id 80h. 41h then
  * selects a board by its id (02h is the Y8960 cartridge) and 42h a function
  * (01h is the Config ROM reader/writer). Each register reads back the
  * complement of the value that selected it.
  *
  * Only this identification is emulated. The Config ROM reader/writer behind
  * function 01h (43h-47h) is not: those ports read FFh and take no writes.
  *
  * The hardware forgets the board and function selection when 40h is written
  * with anything other than 80h. The device switch does not tell a switched
  * device that it was deselected, so here the selection survives until the
  * next write to 41h or a reset.
  */
class Y8960SystemController final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit Y8960SystemController(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime time) override;

	// MSXSwitchedDevice
	[[nodiscard]] uint8_t readSwitchedIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekSwitchedIO(uint16_t port, EmuTime time) const override;
	void writeSwitchedIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool boardSelected = false;
	bool romFunction = false;
};

} // namespace openmsx

#endif
