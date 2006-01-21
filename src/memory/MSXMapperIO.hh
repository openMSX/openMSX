// $Id$

#ifndef MSXMAPPERIO_HH
#define MSXMAPPERIO_HH

#include "MSXDevice.hh"
#include <memory>
#include <set>

namespace openmsx {

class MapperIODebuggable;

class MapperMask
{
public:
	virtual ~MapperMask() {}
	virtual byte calcMask(const std::multiset<unsigned>& mapperSizes) = 0;
};

class MSXMapperIO : public MSXDevice
{
public:
	MSXMapperIO(MSXMotherBoard& motherBoard, const XMLElement& config,
	            const EmuTime& time);
	virtual ~MSXMapperIO();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

	/**
	 * Every MSXMemoryMapper must (un)register its size.
	 * This is used to influence the result returned in readIO().
	 */
	void registerMapper(unsigned blocks);
	void unregisterMapper(unsigned blocks);

	/**
	 * Returns the actual selected page for the given bank.
	 */
	byte getSelectedPage(byte bank) const;

private:
	void write(unsigned address, byte value);
	
	friend class MapperIODebuggable;
	const std::auto_ptr<MapperIODebuggable> debuggable;
	std::auto_ptr<MapperMask> mapperMask;
	std::multiset<unsigned> mapperSizes;
	byte registers[4];
	byte mask;
};

} // namespace openmsx

#endif
