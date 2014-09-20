#ifndef GOUDASCSI_HH
#define GOUDASCSI_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class WD33C93;
class Rom;

class GoudaSCSI final : public MSXDevice
{
public:
	explicit GoudaSCSI(const DeviceConfig& config);
	~GoudaSCSI();

	void reset(EmuTime::param time) override;

	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte readIO(word port, EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<Rom> rom;
	const std::unique_ptr<WD33C93> wd33c93;
};

} // namespace openmsx

#endif
