#ifndef SENSORKID_HH
#define SENSORKID_HH

#include "MSXDevice.hh"

namespace openmsx {

class Rom;

class SensorKid final : public MSXDevice
{
public:
	explicit SensorKid(const DeviceConfig& config);

	void writeIO(word port, byte value, EmuTime::param time) override;

	byte readIO(word port, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<Rom> rom; // can be nullptr

	void putPort(byte data, byte diff);
	byte getAnalog(byte chi);

	byte prev;
	byte mb4052_ana;
	byte mb4052_count;
	byte analog[4];
	byte analog_dir[4];
};

} // namespace openmsx

#endif
