// $Id$

#ifndef __MSXRom_HH__
#define __MSXRom_HH__

#include <string>
#include <memory>
#include "MSXMemDevice.hh"
#include "Rom.hh"

using std::string;
using std::auto_ptr;

namespace openmsx {

class Rom;

class MSXRom : public MSXMemDevice
{
public:
	virtual ~MSXRom();

	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
	virtual const string& getName() const;

protected:
	MSXRom(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom);

	const auto_ptr<Rom> rom;
	static class MSXCPU* cpu;

private:
	void init();
};

} // namespace openmsx

#endif
