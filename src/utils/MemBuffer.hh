// $Id$

#ifndef MEMBUFFER_HH
#define MEMBUFFER_HH

#include <algorithm>
#include <memory>
#include <new>      // for bad_alloc
#include <cstdlib>
#include <cassert>

namespace openmsx {

/** This class manages the lifetime of a block of memory.
  *
  * Its two main use cases are:
  *  1) As a safer alternative for new[] / delete[].
  *     Using this class makes sure the memory block is always properly
  *     cleaned up. Also in case of exceptions.
  *  2) As an alternative for vector<byte> ('byte' or other primitive types).
  *     The main difference with vector is that the allocated block is left
  *     uninitialized. This can be a bit more efficient if the block will
  *     anyway soon be overwritten.
  *
  * Like vector this buffer can dynamically grow or shrink. But it's not
  * optimized for this case (it doesn't keep track of extra capacity). If you
  * need frequent resizing prefer to use vector instead of this class.
  */
template<typename T> class MemBuffer
{
public:
	/** Construct an empty MemBuffer, no memory is allocated.
	 */
	MemBuffer()
		: dat(nullptr, &::free)
		, sz(0)
	{
	}

	/** Construct a (uninitialized) memory buffer of given size.
	 */
	explicit MemBuffer(unsigned size)
		: dat(static_cast<T*>(malloc(size * sizeof(T))), &::free)
		, sz(size)
	{
		if (size && dat) {
			throw std::bad_alloc();
		}
	}

	/** Take ownership of the given memory block. This pointer should have
	 * been allocated earlier with malloc() or realloc() (or it should be
	 * nullptr).
	  */
	MemBuffer(T* data, unsigned size)
		: dat(data, &::free)
		, sz(size)
	{
	}

	// For some reason vs2012 doesn't auto-generate the move-constructor
	// and move assignment operator (gcc does, and I believe the standard
	// says it should).
	MemBuffer(MemBuffer&& other)
		: dat(std::move(other.dat))
		, sz (std::move(other.sz))
	{
	}
	MemBuffer& operator=(MemBuffer&& other)
	{
		dat = std::move(other.dat);
		sz  = std::move(other.sz);
		return *this;
	}

	/** Returns pointer to the start of the memory buffer.
	  * This method can be called even when there's no buffer allocated.
	  */
	const T* data() const { return dat.get(); }
	      T* data()       { return dat.get(); }

	/** Access elements in the memory buffer.
	 */
	const T& operator[](unsigned i) const
	{
		assert(i < sz);
		return dat[i];
	}
	T& operator[](unsigned i)
	{
		assert(i < sz);
		return dat[i];
	}

	/** Returns size of the memory buffer.
	  * The size is in number of elements, not number of allocated bytes.
	  */
	unsigned size() const { return sz; }

	/** Same as size() == 0.
	 */
	bool empty() const { return sz == 0; }

	/** Grow or shrink the memory block.
	  * In case of growing, the extra space is left uninitialized.
	  * It is possible (even likely) that the memory buffer is copied
	  * to a new location after this call, so data() will return a
	  * different pointer value.
	  */
	void resize(unsigned size)
	{
		if (size) {
			auto newDat = static_cast<T*>(realloc(dat.get(), size * sizeof(T)));
			if (!newDat) {
				throw std::bad_alloc();
			}
			dat.release();
			dat.reset(newDat);
			sz = size;
		} else {
			// realloc() can handle zero-size allocactions,
			// but then we anyway still need to check for
			// 'size == 0' for the error handling.
			clear();
		}
	}

	/** Free the allocated memory block and set the current size to 0.
	 */
	void clear()
	{
		dat.reset();
		sz = 0;
	}

private:
	std::unique_ptr<T[], decltype(&::free)> dat;
	unsigned sz;
};

} // namespace openmsx

#endif
