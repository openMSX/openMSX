#ifndef COMMANDDEVICE_HH
#define COMMANDDEVICE_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"

namespace openmsx {

class CommandDevice final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit CommandDevice(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;

	// MSXSwitchedDevice
	byte readSwitchedIO(word port, EmuTime::param time) override;
	byte peekSwitchedIO(word port, EmuTime::param time) const override;
	void writeSwitchedIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void commandExecute();

	std::string commandRequest;
	std::string commandResponse;
	unsigned int commandResponseIndex;
	byte commandStatus;
};

} // namespace openmsx

#endif
