// $Id$

#ifndef __DIRTYCHECKER_HH__
#define __DIRTYCHECKER_HH__

#include "VRAMObserver.hh"
#include <cassert>

#include <stdio.h>
/** Helper class for renderer caches: keeps a record of which VRAM bytes
  * were modified since the last cache validation.
  * The template parameter "unit" determines the unit size in bytes:
  * any VRAM write at an address inside a unit will mark all addresses
  * in that unit as dirty.
  */
template <int unit>
class DirtyChecker : public VRAMObserver
{

public:

	/** Create a new DirtyChecker.
	  * Initially every unit is considered dirty.
	  * @param size The cache size in units.
	  */
	DirtyChecker(int size, const char *name = "unknown");

	virtual ~DirtyChecker();

	/** Validates an entry in the cache:
	  * return dirty flag and mark as not dirty.
	  * @param unitNr Number of unit in cache.
	  * @return true iff this entry was dirty.
	  */
	inline bool validate(int unitNr) {
		assert(0 <= unitNr && unitNr < size);
		bool ret = validFlags[unitNr];
		validFlags[unitNr] = true;
		return ret;
	}

	/** Invalidates the entire cache.
	  */
	void flush();

	void updateVRAM(int offset, const EmuTime &time);

	void updateWindow(bool enabled, const EmuTime &time);

private:

	/** For every unit this array stores its cache state:
	  * valid (true) or dirty (false).
	  */
	bool *validFlags;

	/** Number of units in validFlags array.
	  */
	int size;

	const char *name;

};

#endif // __DIRTYCHECKER_HH__
