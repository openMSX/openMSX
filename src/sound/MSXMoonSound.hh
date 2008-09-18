// $Id$

#ifndef MSXMOONSOUND_HH
#define MSXMOONSOUND_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class YMF262;
class YMF278;

class MSXMoonSound : public MSXDevice
{
public:
	MSXMoonSound(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXMoonSound();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<YMF262> ymf262;
	const std::auto_ptr<YMF278> ymf278;
	int opl3latch;
	byte opl4latch;
};

} // namespace openmsx

#endif
