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
	~ProgrammableDevice();

	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	void reset(EmuTime::param time) override;

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
