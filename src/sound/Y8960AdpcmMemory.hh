#ifndef Y8960ADPCMMEMORY_HH
#define Y8960ADPCMMEMORY_HH

#include "MSXDevice.hh"
#include "TrackedRam.hh"
#include "serialize_meta.hh"

#include <cstdint>

namespace openmsx {

/** The ADPCM sample memory of the Y8960's OPL2 block.
  *
  * The cartridge has one 256kB region of SRAM for ADPCM (40000h-7FFFFh of the
  * serial SRAM). It is a device of its own rather than a member of the OPL2
  * so that it has one owner and one place in the save state, which is how it
  * was laid out while two OPL2 blocks still divided it between them.
  */
class Y8960AdpcmMemory final : public MSXDevice
{
public:
	static constexpr unsigned Size = 256 * 1024;

	explicit Y8960AdpcmMemory(const DeviceConfig& config);

	void powerUp(EmuTime time) override;

	/** Addresses at or past Size read as 0 and writes to them are dropped,
	  * which is what a real chip does where the address space is wider than
	  * the memory behind it. */
	[[nodiscard]] uint8_t read(unsigned addr) const;
	void write(unsigned addr, uint8_t value);
	void clear();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	TrackedRam ram;
};
SERIALIZE_CLASS_VERSION(Y8960AdpcmMemory, 2);

} // namespace openmsx

#endif
