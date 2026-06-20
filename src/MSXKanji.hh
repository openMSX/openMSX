#ifndef MSXKANJI_HH
#define MSXKANJI_HH

#include "MSXDevice.hh"
#include "Rom.hh"

#include <array>
#include <optional>

namespace openmsx {

class MSXKanji final : public MSXDevice
{
public:
	explicit MSXKanji(DeviceConfig& config);

	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;
	void reset(EmuTime time) override;

	void getExtraDeviceInfo(TclObject& result) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct ReadImplResult {
		std::optional<uint8_t> adrLevel; // 0/1 for level1/2, or nullopt for read/write mismatch
		uint8_t value = 0xFF;
	};
	ReadImplResult readImpl(uint16_t port) const;

private:
	enum class Level2Via : uint8_t {
		//OnlyLevel1, // only level 1 is present, level2 ports (if configured) are mirrors for level1
		Read, // Level1 or level2 is decided via the port-set (level1 or 2) used for reading
		      // In the current implementation this mode implies NOT-shared address+counter
		Write, // Level1 or level 2 is decided via the port-set used during writing.
		       //  This mode implies a shared address+counter.
		InterlockedWriteRead, // Write to port-set for level1/2 must be followed by read for the same set.
		                      // Reading from the other port-set returns 0xFF.
	};

	Rom rom;
	std::array<uint32_t, 2> adr; // 2 x 17 bits   (6 row + 6 column + 5 counter), the 18th bit (level) is not included
	                             //  exception: for 'hangul', the address is 18 bit
	                             // Note: adr[1] only used in mode "Level2Via::Read", other modes share adr[0] for level1+2
	uint8_t writeLevel; // '0' for level1, '1' for level2

	uint8_t highAddressMask;
	Level2Via level2Via;
	uint8_t portMask; // '1' if only level1 is present (level2 mirrors level1)
	                  // '3' is both level1 and level2 are present
};

} // namespace openmsx

#endif
