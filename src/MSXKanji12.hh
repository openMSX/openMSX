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
	virtual ~MSXKanji12();

	// MSXDevice
	virtual void reset(EmuTime::param time);

	// MSXSwitchedDevice
	virtual byte readSwitchedIO(word port, EmuTime::param time);
	virtual byte peekSwitchedIO(word port, EmuTime::param time) const;
	virtual void writeSwitchedIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<Rom> rom;
	unsigned address;
};

} // namespace openmsx

#endif
