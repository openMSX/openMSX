// $Id$

#ifndef MEMBUFFER_HH
#define MEMBUFFER_HH

#include "openmsx.hh"

namespace openmsx {

/** Memory buffer.
  * This class manages the lifetime of a block of memory.
  * The memory block can read/written, its length can be queried and it can
  * be grown/shrunk in size.
  */
class MemBuffer
{
public:
	/** Construct an empty MemBuffer (size() == 0).
	 */
	MemBuffer();

	/** Take ownership of the given memory block. This pointer should have
	 * been allocated earlier with malloc() or realloc() (or it should be
	 * NULL).
	  */
	MemBuffer(byte* data, unsigned size);

	/** Free the memory buffer. */
	~MemBuffer();

	/** Returns pointer to the start of the memory buffer. */
	const byte* data() const { return dat; }
	      byte* data()       { return dat; }

	/** Returns size of the memory buffer. */
	unsigned size() const { return sz;  }

	/** Grow or shrink the memory block.
	  * In case of growing, the extra space is left uninitialized.
	  * It is possible (even likely) that the memory buffer is copied
	  * to a new location after this call, so getData() returns a different
	  * value.
	  */
	void resize(unsigned newSize);

private:
	byte* dat;
	unsigned sz;
};

} // namespace openmsx

#endif
