// $Id$

#ifndef MSXROM_HH
#define MSXROM_HH

#include "MSXDevice.hh"
#include <string>
#include <memory>

namespace openmsx {

class Rom;
class MSXCPU;

class MSXRom : public MSXDevice
{
public:
	virtual ~MSXRom();

	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
	virtual const std::string& getName() const;

protected:
	MSXRom(const XMLElement& config, const EmuTime& time,
	       std::auto_ptr<Rom> rom);

	const std::auto_ptr<Rom> rom;
	static class MSXCPU* cpu;

private:
	void init();
};

} // namespace openmsx

#endif
