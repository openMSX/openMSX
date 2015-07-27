#ifndef SENSORKID_HH
#define SENSORKID_HH

#include "MSXDevice.hh"
#include "StringSetting.hh"
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

	StringSetting portStatusCallBackSetting;
	TclCallback portStatusCallback;

	byte prev;
	byte mb4052_ana;
	byte mb4052_count;

	byte analog[4];
	bool analog_dir[4];
};

} // namespace openmsx

#endif
