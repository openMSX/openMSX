#ifndef FRANKY_HH
#define FRANKY_HH

#include "MSXDevice.hh"
#include "SMSVDP.hh"
#include "SN76489.hh"

namespace openmsx {

/** SuperSoniqs Franky cartridge.
  *
  * Contains a Sega Master System VDP (315-5124/315-5246) with its
  * integrated SN76489 PSG. The VDP is visible through I/O ports
  * 0x88/0x89 (data/control), the counters on 0x48/0x49 and the PSG on
  * 0x48/0x49 (write). The VDP interrupt is connected to the slot's /INT.
  */
class Franky final : public MSXDevice
{
public:
	explicit Franky(const DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SMSVDP vdp;
	SN76489 sn76489;
};

} // namespace openmsx

#endif
