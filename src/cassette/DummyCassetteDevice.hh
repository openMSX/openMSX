#ifndef DUMMYCASSETTEDEVICE_HH
#define DUMMYCASSETTEDEVICE_HH

#include "CassetteDevice.hh"

namespace openmsx {

class DummyCassetteDevice final : public CassetteDevice
{
public:
	void setMotor(bool status, EmuTime time) override;
	void setSignal(bool output, EmuTime time) override;
	int16_t readSample(EmuTime time) override;

	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
};

} // namespace openmsx

#endif
