// $Id$

#include "DirtyChecker.hh"
#include "config.h"
#include <cstring>


namespace openmsx {

// Force template instantiation:
template class DirtyChecker<1>;
template class DirtyChecker<8>;

template <int unit>
DirtyChecker<unit>::DirtyChecker(int size, const char *name) {
	this->size = size;
	this->name = name;
	validFlags = new bool[size];
	flush();
}

template <int unit>
DirtyChecker<unit>::~DirtyChecker() {
	delete[] validFlags;
}

template <int unit>
void DirtyChecker<unit>::flush() {
	//fprintf(stderr, "Flush(%s) ", name);
	memset(validFlags, 0, size * SIZEOF_BOOL);
}

template <int unit>
void DirtyChecker<unit>::updateVRAM(int offset, const EmuTime &time) {
	int unitNr = offset / unit;
	assert(0 <= unitNr && unitNr < size);
	validFlags[unitNr] = false;
}

template <int unit>
void DirtyChecker<unit>::updateWindow(bool enabled, const EmuTime &time) {
	if (enabled) flush();
}

} // namespace openmsx

