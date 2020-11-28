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

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SRAM sram;
	RP5C01 rp5c01;
	nibble registerLatch;
};

} // namespace openmsx

#endif
