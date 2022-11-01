#ifndef ALIGNEDBUFFER_HH
#define ALIGNEDBUFFER_HH

#include <array>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace openmsx {

// Interface for an aligned buffer.
// This type doesn't itself provide any storage, it only 'refers' to some
// storage. In that sense it is very similar in usage to a 'uint8_t*'.
//
// For example:
//   void f1(uint8_t* buf) {
//     ... uint8_t x = buf[7];
//     ... buf[4] = 10;
//     ... uint8_t* begin = buf;
//     ... uint8_t* end   = buf + 12;
//   }
//   void f2(AlignedBuffer& buf) {
//     // The exact same syntax as above, the only difference is that here
//     // the compiler knows at compile-time the alignment of 'buf', while
//     // above the compiler must assume worst case alignment.
//   }
class alignas(std::max_align_t) AlignedBuffer
{
private:
	[[nodiscard]] auto* p()       { return reinterpret_cast<      uint8_t*>(this); }
	[[nodiscard]] auto* p() const { return reinterpret_cast<const uint8_t*>(this); }

public:
	static constexpr auto ALIGNMENT = alignof(std::max_align_t);

	[[nodiscard]] operator       uint8_t*()       { return p(); }
	[[nodiscard]] operator const uint8_t*() const { return p(); }
	[[nodiscard]] const auto* data() const { return p(); }
	[[nodiscard]]       auto* data()       { return p(); }

	[[nodiscard]] auto* operator+(ptrdiff_t i)       { return p() + i; }
	[[nodiscard]] auto* operator+(ptrdiff_t i) const { return p() + i; }
	[[nodiscard]] auto* operator+(size_t i)          { return p() + i; }
	[[nodiscard]] auto* operator+(size_t i)    const { return p() + i; }
	[[nodiscard]] auto* operator+(int i)             { return p() + i; }
	[[nodiscard]] auto* operator+(int i)       const { return p() + i; }
	[[nodiscard]] auto* operator+(unsigned i)        { return p() + i; }
	[[nodiscard]] auto* operator+(unsigned i)  const { return p() + i; }

	[[nodiscard]] auto& operator[](int           i)       { return *(p() + i); }
	[[nodiscard]] auto& operator[](int           i) const { return *(p() + i); }
	[[nodiscard]] auto& operator[](unsigned int  i)       { return *(p() + i); }
	[[nodiscard]] auto& operator[](unsigned int  i) const { return *(p() + i); }
	[[nodiscard]] auto& operator[](long          i)       { return *(p() + i); }
	[[nodiscard]] auto& operator[](long          i) const { return *(p() + i); }
	[[nodiscard]] auto& operator[](unsigned long i)       { return *(p() + i); }
	[[nodiscard]] auto& operator[](unsigned long i) const { return *(p() + i); }
};
static_assert(alignof(AlignedBuffer) == AlignedBuffer::ALIGNMENT, "must be aligned");


// Provide actual storage for the AlignedBuffer
// A possible alternative is to use a union.
template<size_t N> class AlignedByteArray : public AlignedBuffer
{
public:
	[[nodiscard]] auto  size()  const { return dat.size(); }
	[[nodiscard]] auto* data()        { return dat.data(); }
	[[nodiscard]] auto* data()  const { return dat.data(); }
	[[nodiscard]] auto  begin()       { return dat.begin(); }
	[[nodiscard]] auto  begin() const { return dat.begin(); }
	[[nodiscard]] auto  end()         { return dat.end(); }
	[[nodiscard]] auto  end()   const { return dat.end(); }

private:
	std::array<uint8_t, N> dat;

};
static_assert(alignof(AlignedByteArray<13>) == AlignedBuffer::ALIGNMENT,
              "must be aligned");
static_assert(sizeof(AlignedByteArray<32>) == 32,
              "we rely on the empty-base optimization");


/** Cast one pointer type to another pointer type.
  * When asserts are enabled this checks whether the original pointer is
  * properly aligned to point to the destination type.
  */
template<typename T> [[nodiscard]] static inline T aligned_cast(void* p)
{
	static_assert(std::is_pointer_v<T>,
	              "can only perform aligned_cast on pointers");
	assert((reinterpret_cast<uintptr_t>(p) %
	        alignof(std::remove_pointer_t<T>)) == 0);
	return reinterpret_cast<T>(p);
}

} //namespace openmsx

#endif
