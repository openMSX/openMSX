// $Id$

#ifndef MSXOPL3CARTRIDGE_HH
#define MSXOPL3CARTRIDGE_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class YMF262;

class MSXOPL3Cartridge : public MSXDevice
{
public:
	MSXOPL3Cartridge(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXOPL3Cartridge();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<YMF262> ymf262;
	int opl3latch;
};

} // namespace openmsx

#endif
