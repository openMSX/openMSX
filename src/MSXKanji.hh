#ifndef MSXKANJI_HH
#define MSXKANJI_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;

class MSXKanji final : public MSXDevice
{
public:
	explicit MSXKanji(const DeviceConfig& config);
	~MSXKanji();

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<Rom> rom;
	unsigned adr1, adr2;
	const bool isLascom;
};

} // namespace openmsx

#endif
