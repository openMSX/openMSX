#ifndef SENSORKID_HH
#define SENSORKID_HH

#include "MSXDevice.hh"
#include "TclCallback.hh"

namespace openmsx {

class SensorKid final : public MSXDevice
{
public:
	explicit SensorKid(const DeviceConfig& config);

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void putPort(byte data, byte diff) const;
	[[nodiscard]] byte getAnalog(byte chi) const;

private:
	TclCallback portStatusCallback;
	TclCallback acquireCallback;

	byte prev;
	byte mb4052_ana;
	byte mb4052_count;
};

} // namespace openmsx

#endif
