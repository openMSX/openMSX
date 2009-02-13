// $Id$

#ifndef NOWINDINTERFACE_HH
#define NOWINDINTERFACE_HH

#include "MSXDevice.hh"
#include <memory>
#include <vector>

namespace openmsx {

class NowindCommand;
class Rom;
class AmdFlash;
class DiskChanger;
class DiskContainer;
class NowindHost;

class NowindInterface : public MSXDevice
{
public:
	NowindInterface(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~NowindInterface();

	virtual void reset(EmuTime::param time);
	virtual byte peek(word address, EmuTime::param time) const;
	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<NowindCommand> command;
	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<AmdFlash> flash;
	typedef std::vector<DiskContainer*> Drives;
	Drives drives;
	const std::auto_ptr<NowindHost> host;
	byte bank;

	friend class NowindCommand;
};

} // namespace openmsx

#endif
