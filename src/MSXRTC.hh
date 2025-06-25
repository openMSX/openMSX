#ifndef MSXRTC_HH
#define MSXRTC_HH

#include "MSXDevice.hh"
#include "SRAM.hh"
#include "RP5C01.hh"

namespace openmsx {

class MSXRTC final : public MSXDevice
{
public:
	explicit MSXRTC(const DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SRAM sram;
	RP5C01 rp5c01;
	uint4_t registerLatch;
};

} // namespace openmsx

#endif
