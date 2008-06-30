// $Id$

#ifndef MSXKANJI_HH
#define MSXKANJI_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;

class MSXKanji : public MSXDevice
{
public:
	MSXKanji(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXKanji();

	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);
	virtual void reset(const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<Rom> rom;
	unsigned adr1, adr2;
};

REGISTER_MSXDEVICE(MSXKanji, "Kanji");

} // namespace openmsx

#endif
