#ifndef YMF278B_HH
#define YMF278B_HH

#include "YMF262.hh"
#include "YMF278.hh"

#include "serialize.hh"

namespace openmsx {

// This combines the FM-part (YMF262) and the Wave-part (YMF278) into a single chip.
class YMF278B
{
public:
	YMF278B(const std::string& name, size_t ramSize, const DeviceConfig& config,
	        YMF278::SetupMemPtrFunc setupMemPtrs, EmuTime::param time);

	void powerUp(EmuTime::param time);
	void reset(EmuTime::param time);
	[[nodiscard]] byte readIO(word port, EmuTime::param time);
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const;
	void writeIO(word port, byte value, EmuTime::param time);
	void setupMemoryPointers();

	void serialize_bw_compat(XmlInputArchive& ar, unsigned version, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool getNew2() const;
	[[nodiscard]] byte readYMF278Status(EmuTime::param time) const;

private:
	YMF262 ymf262;
	YMF278 ymf278;

	/** Time at which instrument loading is finished. */
	EmuTime ymf278LoadTime;
	/** Time until which the YMF278 is busy. */
	EmuTime ymf278BusyTime;

	int opl3latch = 0;
	byte opl4latch = 0;
};

} // namespace openmsx

#endif
