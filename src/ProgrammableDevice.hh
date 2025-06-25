#ifndef PROGRAMMABLEDEVICE_HH
#define PROGRAMMABLEDEVICE_HH

#include "MSXDevice.hh"
#include "StringSetting.hh"
#include "TclCallback.hh"

namespace openmsx {

class ProgrammableDevice final : public MSXDevice
                               , private Observer<Setting>
{
public:
	explicit ProgrammableDevice(const DeviceConfig& config);
	~ProgrammableDevice() override;

	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	void reset(EmuTime time) override;

private:
	void update(const Setting& setting) noexcept override;

private:
	StringSetting portListSetting;
	TclCallback resetCallback;
	TclCallback inCallback;
	TclCallback outCallback;
};

} // namespace openmsx

#endif
