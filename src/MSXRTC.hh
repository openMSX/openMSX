// $Id$

#ifndef __MSXRTC_HH__
#define __MSXRTC_HH__

#include <memory>
#include "MSXDevice.hh"
#include "SRAM.hh"

using std::auto_ptr;

namespace openmsx {

class RP5C01;

class MSXRTC : public MSXDevice
{
public:
	MSXRTC(const XMLElement& config, const EmuTime& time);
	virtual ~MSXRTC(); 

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	auto_ptr<RP5C01> rp5c01;
	SRAM sram;
	nibble registerLatch;
};

} // namespace openmsx

#endif
