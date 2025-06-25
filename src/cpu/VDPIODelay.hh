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

	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	[[nodiscard]] const MSXDevice& getInDevice(uint8_t port) const;
	[[nodiscard]] MSXDevice*& getInDevicePtr (uint8_t port);
	[[nodiscard]] MSXDevice*& getOutDevicePtr(uint8_t port);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void delay(EmuTime time);

private:
	MSXCPU& cpu;
	std::array<MSXDevice*, 4> inDevices;
	std::array<MSXDevice*, 4> outDevices;
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<7159090> lastTime{EmuTime::zero()};
};

} // namespace openmsx

#endif
