/*
 * This class implements the device found on IO-port 0xF4 on a MSX Turbo-R.
 *
 *  TODO explanation
 */

#ifndef F4DEVICE_HH
#define F4DEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXResetStatusRegister : public MSXDevice
{
public:
	explicit MSXResetStatusRegister(const DeviceConfig& config);

	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const bool inverted;
	byte status;
};

} // namespace openmsx

#endif
