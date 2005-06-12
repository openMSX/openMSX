// $Id$

#ifndef MSXRTC_HH
#define MSXRTC_HH

#include <memory>
#include "MSXDevice.hh"

namespace openmsx {

class SRAM;
class RP5C01;

class MSXRTC : public MSXDevice
{
public:
	MSXRTC(const XMLElement& config, const EmuTime& time);
	virtual ~MSXRTC();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	std::auto_ptr<SRAM> sram;
	std::auto_ptr<RP5C01> rp5c01;
	nibble registerLatch;
};

} // namespace openmsx

#endif
