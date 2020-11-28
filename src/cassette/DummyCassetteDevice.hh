#ifndef DUMMYCASSETTEDEVICE_HH
#define DUMMYCASSETTEDEVICE_HH

#include "CassetteDevice.hh"

namespace openmsx {

class DummyCassetteDevice final : public CassetteDevice
{
public:
	void setMotor(bool status, EmuTime::param time) override;
	void setSignal(bool output, EmuTime::param time) override;
	int16_t readSample(EmuTime::param time) override;

	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
};

} // namespace openmsx

#endif
