#ifndef MSXVICTORHC9XSYSTEMCONTROL_HH
#define MSXVICTORHC9XSYSTEMCONTROL_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXVictorHC9xSystemControl final : public MSXDevice
{
public:
	explicit MSXVictorHC9xSystemControl(const DeviceConfig& config);
	~MSXVictorHC9xSystemControl();

	virtual byte readMem(word address, EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte systemControlRegister;

};

} // namespace openmsx

#endif
