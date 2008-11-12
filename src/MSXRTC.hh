// $Id$

#ifndef MSXRTC_HH
#define MSXRTC_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SRAM;
class RP5C01;

class MSXRTC : public MSXDevice
{
public:
	MSXRTC(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXRTC();

	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<SRAM> sram;
	const std::auto_ptr<RP5C01> rp5c01;
	nibble registerLatch;
};

} // namespace openmsx

#endif
