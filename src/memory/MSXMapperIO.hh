// $Id$

#ifndef __MSXMAPPERIO_HH__
#define __MSXMAPPERIO_HH__

#include <set>
#include <memory>
#include "openmsx.hh"
#include "MSXDevice.hh"

namespace openmsx {

class MapperMask
{
public:
	virtual ~MapperMask() {}
	virtual byte calcMask(const std::multiset<unsigned>& mapperSizes) = 0;
};

class MSXMapperIO : public MSXDevice
{
public:
	MSXMapperIO(const XMLElement& config, const EmuTime& time);
	virtual ~MSXMapperIO();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);
	
	/**
	 * Every MSXMemoryMapper must (un)register its size.
	 * This is used to influence the result returned in readIO().
	 */
	void registerMapper(unsigned blocks);
	void unregisterMapper(unsigned blocks);
	
	/**
	 * Returns the actual selected page for the given bank.
	 */
	byte getSelectedPage(byte bank);

private:
	std::auto_ptr<MapperMask> mapperMask;
	std::multiset<unsigned> mapperSizes;
	byte mask;
	byte page[4];
};

} // namespace openmsx

#endif
