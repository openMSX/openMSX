// $Id$

#ifndef MSXS1990_HH
#define MSXS1990_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class FirmwareSwitch;

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
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	void setCPUStatus(byte value);

	const std::auto_ptr<FirmwareSwitch> firmwareSwitch;
	byte registerSelect;
	byte cpuStatus;
};

} // namespace openmsx

#endif
