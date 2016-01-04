#ifndef MEMBUFFER_HH
#define MEMBUFFER_HH

#include "MemoryOps.hh"
#include "alignof.hh"
#include <algorithm>
#include <new>      // for bad_alloc
#include <cstdlib>
#include <cassert>

namespace openmsx {

// When SSE2 is enabled (some) buffers need to be 16-bytes aligned. If not
// then don't enforce stricter than default alignment.
#ifdef __SSE2__
static const size_t SSE2_ALIGNMENT = 16;
#else
static const size_t SSE2_ALIGNMENT = 0;
#endif


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
template<typename T, size_t ALIGNMENT = 0> class MemBuffer
{
public:
	/** Construct an empty MemBuffer, no memory is allocated.
	 */
	MemBuffer()
		: dat(nullptr)
#ifdef DEBUG
		, sz(0)
#endif
	{
	}

	/** Construct a (uninitialized) memory buffer of given size.
	 */
	explicit MemBuffer(size_t size)
		: dat(static_cast<T*>(my_malloc(size * sizeof(T))))
#ifdef DEBUG
		, sz(size)
#endif
	{
	}

	/** Move constructor. */
	MemBuffer(MemBuffer&& other) noexcept
		: dat(other.dat)
#ifdef DEBUG
		, sz(other.sz)
#endif
	{
		other.dat = nullptr;
	}

	/** Move assignment. */
	MemBuffer& operator=(MemBuffer&& other) noexcept
	{
		std::swap(dat, other.dat);
#ifdef DEBUG
		std::swap(sz , other.sz);
#endif
		return *this;
	}

	/** Free the memory buffer.
	 */
	~MemBuffer()
	{
		my_free(dat);
	}

	/** Returns pointer to the start of the memory buffer.
	  * This method can be called even when there's no buffer allocated.
	  */
	const T* data() const { return dat; }
	      T* data()       { return dat; }

	/** Access elements in the memory buffer.
	 */
	const T& operator[](size_t i) const
	{
#ifdef DEBUG
		assert(i < sz);
#endif
		return dat[i];
	}
	T& operator[](size_t i)
	{
#ifdef DEBUG
		assert(i < sz);
#endif
		return dat[i];
	}

	/** No memory allocated?
	 */
	bool empty() const { return !dat; }

	/** Grow or shrink the memory block.
	  * In case of growing, the extra space is left uninitialized.
	  * It is possible (even likely) that the memory buffer is copied
	  * to a new location after this call, so data() will return a
	  * different pointer value.
	  */
	void resize(size_t size)
	{
		if (size) {
			dat = static_cast<T*>(my_realloc(dat, size * sizeof(T)));
#ifdef DEBUG
			sz = size;
#endif
		} else {
			clear();
		}
	}

	/** Free the allocated memory block and set the current size to 0.
	 */
	void clear()
	{
		my_free(dat);
		dat = nullptr;
#ifdef DEBUG
		sz = 0;
#endif
	}

	/** Swap the managed memory block of two MemBuffers.
	 */
	void swap(MemBuffer& other) noexcept
	{
		std::swap(dat, other.dat);
#ifdef DEBUG
		std::swap(sz , other.sz );
#endif
	}

private:
	// If the requested alignment is less or equally strict than the
	// guaranteed alignment by the standard malloc()-like functions
	// we use those. Otherwise we use platform specific functions to
	// request aligned memory.
	// A valid alternative would be to always use the platform specific
	// functions. The only disadvantage is that we cannot use realloc()
	// in that case (there are no, not even platform specific, functions
	// to realloc memory with bigger than default alignment).
	static const bool SIMPLE_MALLOC = ALIGNMENT <= ALIGNOF(MAX_ALIGN_T);

	void* my_malloc(size_t bytes)
	{
		void* result;
		if (SIMPLE_MALLOC) {
			result = malloc(bytes);
			if (!result && bytes) throw std::bad_alloc();
		} else {
			// already throws bad_alloc in case of error
			result = MemoryOps::mallocAligned(ALIGNMENT, bytes);
		}
		return result;
	}

	void my_free(void* p)
	{
		if (SIMPLE_MALLOC) {
			free(p);
		} else {
			MemoryOps::freeAligned(p);
		}
	}

	void* my_realloc(void* old, size_t bytes)
	{
		void* result;
		if (SIMPLE_MALLOC) {
			result = realloc(old, bytes);
			if (!result && bytes) throw std::bad_alloc();
		} else {
			result = MemoryOps::mallocAligned(ALIGNMENT, bytes);
			if (!result && bytes) throw std::bad_alloc();
			MemoryOps::freeAligned(old);
		}
		return result;
	}

private:
	T* dat;
#ifdef DEBUG
	size_t sz;
#endif
};

} // namespace openmsx

namespace std {
	template<typename T>
	void swap(openmsx::MemBuffer<T>& l, openmsx::MemBuffer<T>& r) noexcept
	{
		l.swap(r);
	}
}

#endif
