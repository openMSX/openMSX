#ifndef MSXS1990_HH
#define MSXS1990_HH

#include "MSXDevice.hh"
#include "FirmwareSwitch.hh"
#include "SimpleDebuggable.hh"

namespace openmsx {

/**
 * This class implements the MSX-engine found in a MSX Turbo-R (S1990)
 *
 * TODO explanation
 */
class MSXS1990 final : public MSXDevice
{
public:
	explicit MSXS1990(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	[[nodiscard]] byte readRegister(byte reg) const;
	void writeRegister(byte reg, byte value);
	void setCPUStatus(byte value);

private:
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debuggable;

	FirmwareSwitch firmwareSwitch;
	byte registerSelect;
	byte cpuStatus;
};

} // namespace openmsx

#endif
