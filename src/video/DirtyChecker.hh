// $Id$

#ifndef __DIRTYCHECKER_HH__
#define __DIRTYCHECKER_HH__

#include <cassert>
#include <bitset>
#include "VRAMObserver.hh"

using std::bitset;

namespace openmsx {

/** Helper class for renderer caches: keeps a record of which VRAM bytes
  * were modified since the last cache validation.
  * The template parameter "size" determines the cache size in units.
  * The template parameter "unit" determines the unit size in bytes:
  * any VRAM write at an address inside a unit will mark all addresses
  * in that unit as dirty.
  * Initially every unit is considered dirty.
  */
template <unsigned size, unsigned unit>
class DirtyChecker : public VRAMObserver
{
public:
	/** Validates an entry in the cache:
	  * return valid/dirty flag and mark as valid.
	  * @param unitNr Number of unit in cache.
	  * @return true iff this entry was valid.
	  */
	inline bool validate(unsigned unitNr) {
		assert(unitNr < size);
		bool ret = validFlags[unitNr];
		validFlags[unitNr] = true;
		return ret;
	}

	/** Invalidates the entire cache.
	  */
	void flush();

	// VRAMObserver interface
	virtual void updateVRAM(unsigned offset, const EmuTime& time);
	virtual void updateWindow(bool enabled, const EmuTime& time);

private:
	/** For every unit this array stores its cache state:
	  * valid (true) or dirty (false).
	  */
	bitset<size> validFlags;
};


template <unsigned size, unsigned unit>
void DirtyChecker<size, unit>::flush()
{
	validFlags.reset();
}

template <unsigned size, unsigned unit>
void DirtyChecker<size, unit>::updateVRAM(unsigned offset,
                                          const EmuTime& /*time*/)
{
	unsigned unitNr = offset / unit;
	assert(unitNr < size);
	validFlags[unitNr] = false;
}

template <unsigned size, unsigned unit>
void DirtyChecker<size, unit>::updateWindow(bool enabled,
                                            const EmuTime& /*time*/)
{
	if (enabled) {
		flush();
	}
}

} // namespace openmsx

#endif // __DIRTYCHECKER_HH__
