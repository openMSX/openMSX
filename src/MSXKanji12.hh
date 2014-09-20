#ifndef MSXKANJI12_HH
#define MSXKANJI12_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
#include <memory>

namespace openmsx {

class Rom;

class MSXKanji12 final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXKanji12(const DeviceConfig& config);
	~MSXKanji12();

	// MSXDevice
	void reset(EmuTime::param time) override;

	// MSXSwitchedDevice
	byte readSwitchedIO(word port, EmuTime::param time) override;
	byte peekSwitchedIO(word port, EmuTime::param time) const override;
	void writeSwitchedIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<Rom> rom;
	unsigned address;
};

} // namespace openmsx

#endif
