// $Id$

#ifndef MSXMAPPERIO_HH
#define MSXMAPPERIO_HH

#include "MSXDevice.hh"
#include <memory>
#include <set>

namespace openmsx {

class MapperIODebuggable;

class MSXMapperIO : public MSXDevice
{
public:
	MSXMapperIO(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXMapperIO();

	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void write(unsigned address, byte value);

	/**
	 * Updates the "mask" field after a mapper was registered or unregistered.
	 */
	void updateMask();

	friend class MapperIODebuggable;
	const std::auto_ptr<MapperIODebuggable> debuggable;
	std::multiset<unsigned> mapperSizes;
	byte registers[4];

	/**
	 * The limit on which bits can be read back as imposed by the engine.
	 * The S1990 engine of the MSX turbo R has such a limit, but other engines
	 * do not.
	 */
	byte engineMask;

	/**
	 * Effective read mask: a combination of engineMask and the limit imposed
	 * by the sizes of the inserted mappers.
	 */
	byte mask;
};

} // namespace openmsx

#endif
