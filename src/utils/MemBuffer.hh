#ifndef MEMBUFFER_HH
#define MEMBUFFER_HH

#include "MemoryOps.hh"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <new>      // for bad_alloc
#include <span>
#include <type_traits>

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
template<typename T, size_t ALIGNMENT = 0> class MemBuffer
{
	static_assert(std::is_trivially_copyable_v<T>);
	static_assert(std::is_trivially_destructible_v<T>);
public:
	/** Construct an empty MemBuffer, no memory is allocated.
	 */
	MemBuffer()
		: dat(nullptr)
		, sz(0)
	{
	}

	/** Construct a (uninitialized) memory buffer of given size.
	 */
	explicit MemBuffer(size_t size)
		: dat(static_cast<T*>(my_malloc(size * sizeof(T))))
		, sz(size)
	{
	}

	/** Move constructor. */
	MemBuffer(MemBuffer&& other) noexcept
		: dat(other.dat)
		, sz(other.sz)
	{
		other.dat = nullptr;
		other.sz = 0;
	}

	/** Move assignment. */
	MemBuffer& operator=(MemBuffer&& other) noexcept
	{
		std::swap(dat, other.dat);
		std::swap(sz , other.sz);
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
	[[nodiscard]] const T* data() const { return dat; }
	[[nodiscard]]       T* data()       { return dat; }

	[[nodiscard]]       T* begin()       { return data(); }
	[[nodiscard]] const T* begin() const { return data(); }
	[[nodiscard]]       T* end()         { return data() + size(); }
	[[nodiscard]] const T* end()   const { return data() + size(); }

	/** Access elements in the memory buffer.
	 */
	[[nodiscard]] const T& operator[](size_t i) const
	{
		assert(i < sz);
		return dat[i];
	}
	[[nodiscard]] T& operator[](size_t i)
	{
		assert(i < sz);
		return dat[i];
	}

	[[nodiscard]] bool empty() const { return sz == 0; }
	[[nodiscard]] size_t size() const { return sz; }

	[[nodiscard]] T& front()
	{
		assert(!empty());
		return dat[0];
	}
	[[nodiscard]] const T& front() const
	{
		assert(!empty());
		return dat[0];
	}
	[[nodiscard]] T& back()
	{
		assert(!empty());
		return dat[sz - 1];
	}
	[[nodiscard]] const T& back() const
	{
		assert(!empty());
		return dat[sz - 1];
	}

	[[nodiscard]] operator std::span<      T>()       { return {data(), size()}; }
	[[nodiscard]] operator std::span<const T>() const { return {data(), size()}; }

	[[nodiscard]] std::span<const T> first(size_t n) const
	{
		assert(n <= sz);
		return std::span{*this}.first(n);
	}
	[[nodiscard]] std::span<T> first(size_t n)
	{
		assert(n <= sz);
		return std::span{*this}.first(n);
	}
	[[nodiscard]] std::span<const T> subspan(size_t offset, size_t n = std::dynamic_extent) const
	{
		assert(offset <= sz);
		assert(n == std::dynamic_extent || (offset + n <= sz));
		return std::span{*this}.subspan(offset, n);
	}
	[[nodiscard]] std::span<T> subspan(size_t offset, size_t n = std::dynamic_extent)
	{
		assert(offset <= sz);
		assert(n == std::dynamic_extent || (offset + n <= sz));
		return std::span{*this}.subspan(offset, n);
	}

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
			sz = size;
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
		sz = 0;
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
	static constexpr bool SIMPLE_MALLOC = ALIGNMENT <= alignof(std::max_align_t);

	[[nodiscard]] static void* my_malloc(size_t bytes)
	{
		if constexpr (SIMPLE_MALLOC) {
			void* result = malloc(bytes);
			if (!result && bytes) throw std::bad_alloc();
			return result;
		} else {
			// already throws bad_alloc in case of error
			return MemoryOps::mallocAligned(ALIGNMENT, bytes);
		}
	}

	static void my_free(void* p)
	{
		if constexpr (SIMPLE_MALLOC) {
			free(p);
		} else {
			MemoryOps::freeAligned(p);
		}
	}

	[[nodiscard]] void* my_realloc(void* old, size_t bytes)
	{
		if constexpr (SIMPLE_MALLOC) {
			void* result = realloc(old, bytes);
			if (!result && bytes) throw std::bad_alloc();
			return result;
		} else {
			void* result = MemoryOps::mallocAligned(ALIGNMENT, bytes);
			if (!result && bytes) throw std::bad_alloc();
			MemoryOps::freeAligned(old);
			return result;
		}
	}

private:
	T* dat;
	size_t sz;
};

} // namespace openmsx

#endif
