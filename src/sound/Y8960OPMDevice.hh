#ifndef Y8960OPMDEVICE_HH
#define Y8960OPMDEVICE_HH

#include "MSXDevice.hh"
#include "YM2151.hh"
#include "serialize_meta.hh"

namespace openmsx {

/** The Y8960's OPM (YM2151). Its I/O ports are 7Eh (address) and 7Fh
  * (data). Reading 7Fh returns the status; 7Eh answers no reads. */
class Y8960OPMDevice final : public MSXDevice
{
public:
	explicit Y8960OPMDevice(const DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	void writePort(bool port, byte value, EmuTime time);

	/** Open or close the I/O ports: 7FFFh (I/O Enabler2). writePort(),
	  * from the memory mapped I/O, is not affected. */
	void setIoEnabled(bool enabled) { ioEnabled = enabled; }
	[[nodiscard]] bool isIoEnabled() const { return ioEnabled; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	YM2151 ym2151;
	byte registerLatch;
	bool ioEnabled;
};
SERIALIZE_CLASS_VERSION(Y8960OPMDevice, 1);

} // namespace openmsx

#endif
