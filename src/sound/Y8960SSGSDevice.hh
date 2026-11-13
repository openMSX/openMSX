#ifndef Y8960SSGSDEVICE_HH
#define Y8960SSGSDEVICE_HH

#include "MSXDevice.hh"
#include "Y8960SSGS.hh"
#include "serialize_meta.hh"

namespace openmsx {

/** The Y8960's SSGS block on its I/O ports.
  *
  *   base+0  write the register number
  *   base+1  write the register
  *
  * Same layout as the MSX PSG, but the register number is 6 bits wide here
  * because there are two units. The cartridge answers A2h reads of the
  * second unit's registers, but this device answers no reads: taking A2h as
  * an input overlaps the MSX PSG, and openMSX warns about every overlapping
  * input port each time the extension is inserted.
  */
class Y8960SSGSDevice final : public MSXDevice
{
public:
	explicit Y8960SSGSDevice(const DeviceConfig& config);

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	void writePort(bool port, byte value, EmuTime time);

	/** 7FFFh (I/O Enabler2) bit 1. */
	void setIoEnabled(bool enabled) { ioEnabled = enabled; }
	[[nodiscard]] bool isIoEnabled() const { return ioEnabled; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte registerLatch;
	bool ioEnabled;
	Y8960SSGS ssgs;
};
SERIALIZE_CLASS_VERSION(Y8960SSGSDevice, 3);

} // namespace openmsx

#endif
