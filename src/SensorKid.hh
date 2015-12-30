#ifndef SENSORKID_HH
#define SENSORKID_HH

#include "MSXDevice.hh"
#include "TclCallback.hh"

namespace openmsx {

class SensorKid final : public MSXDevice
{
public:
	explicit SensorKid(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void putPort(byte data, byte diff);
	byte getAnalog(byte chi);

	TclCallback portStatusCallback;
	TclCallback acquireCallback;

	byte prev;
	byte mb4052_ana;
	byte mb4052_count;
};

} // namespace openmsx

#endif
