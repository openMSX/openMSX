#ifndef VDPIODELAY_HH
#define VDPIODELAY_HH

#include "MSXDevice.hh"
#include "Clock.hh"
#include <array>

namespace openmsx {

class MSXCPU;
class MSXCPUInterface;

class VDPIODelay final : public MSXDevice
{
public:
	VDPIODelay(const DeviceConfig& config, MSXCPUInterface& cpuInterface);

	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	[[nodiscard]] const MSXDevice& getInDevice(byte port) const;
	[[nodiscard]] MSXDevice*& getInDevicePtr (byte port);
	[[nodiscard]] MSXDevice*& getOutDevicePtr(byte port);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void delay(EmuTime::param time);

private:
	MSXCPU& cpu;
	std::array<MSXDevice*, 4> inDevices;
	std::array<MSXDevice*, 4> outDevices;
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<7159090> lastTime;
};

} // namespace openmsx

#endif
