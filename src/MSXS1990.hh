// $Id$

#ifndef MSXS1990_HH
#define MSXS1990_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class FirmwareSwitch;
class S1990Debuggable;

/**
 * This class implements the MSX-engine found in a MSX Turbo-R (S1990)
 *
 * TODO explanation
 */
class MSXS1990 : public MSXDevice
{
public:
	MSXS1990(MSXMotherBoard& motherBoard, const XMLElement& config,
	         const EmuTime& time);
	virtual ~MSXS1990();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

private:
	byte readRegister(byte reg) const;
	void writeRegister(byte reg, byte value);
	void setCPUStatus(byte value);

	const std::auto_ptr<FirmwareSwitch> firmwareSwitch;
	const std::auto_ptr<S1990Debuggable> debuggable;
	byte registerSelect;
	byte cpuStatus;

	friend class S1990Debuggable;
};

} // namespace openmsx

#endif
