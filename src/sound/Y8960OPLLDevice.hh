#ifndef Y8960OPLLDEVICE_HH
#define Y8960OPLLDEVICE_HH

#include "MSXDevice.hh"
#include "Y8960OPLL.hh"
#include "serialize_meta.hh"

namespace openmsx {

/** The Y8960's OPLL block on its I/O ports. */
class Y8960OPLLDevice final : public MSXDevice
{
public:
	explicit Y8960OPLLDevice(const DeviceConfig& config);

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	void writePort(bool port, byte value, EmuTime time);

	/** Open or close the I/O ports: 7FF6h (I/O Enabler1). writePort(),
	  * from the memory mapped I/O, is not affected. On the cartridge that
	  * way is always open too; otherwise nothing could open the ports. */
	void setIoEnabled(bool enabled) { ioEnabled = enabled; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Y8960OPLL opll;
	bool ioEnabled;
};
SERIALIZE_CLASS_VERSION(Y8960OPLLDevice, 1);

} // namespace openmsx

#endif
