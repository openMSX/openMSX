#ifndef YMF278B_HH
#define YMF278B_HH

#include "YMF262.hh"
#include "YMF278.hh"

#include "serialize.hh"

#include <cstdint>

namespace openmsx {

// This combines the FM-part (YMF262) and the Wave-part (YMF278) into a single chip.
class YMF278B
{
public:
	YMF278B(const std::string& name, size_t ramSize, DeviceConfig& config,
	        YMF278::SetupMemPtrFunc setupMemPtrs, EmuTime time);

	void powerUp(EmuTime time);
	void reset(EmuTime time);
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time);
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const;
	void writeIO(uint16_t port, uint8_t value, EmuTime time);
	void setupMemoryPointers();

	void serialize_bw_compat(XmlInputArchive& ar, unsigned version, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool getNew2() const;
	[[nodiscard]] uint8_t readYMF278Status(EmuTime time) const;

private:
	YMF262 ymf262;
	YMF278 ymf278;

	/** Time at which instrument loading is finished. */
	EmuTime ymf278LoadTime;
	/** Time until which the YMF278 is busy. */
	EmuTime ymf278BusyTime;

	int opl3latch = 0;
	uint8_t opl4latch = 0;
};

} // namespace openmsx

#endif
