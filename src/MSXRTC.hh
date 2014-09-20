#ifndef MSXRTC_HH
#define MSXRTC_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SRAM;
class RP5C01;

class MSXRTC final : public MSXDevice
{
public:
	explicit MSXRTC(const DeviceConfig& config);
	~MSXRTC();

	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<SRAM> sram;
	const std::unique_ptr<RP5C01> rp5c01;
	nibble registerLatch;
};

} // namespace openmsx

#endif
