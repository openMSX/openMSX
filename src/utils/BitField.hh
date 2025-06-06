#ifndef BITFIELD_HH
#define BITFIELD_HH

#include <concepts>
#include <cstddef>
#include <limits>

// "Portable" bitfield class
//
// This is inspired by:
//   https://blog.codef00.com/2014/12/06/portable-bitfields-using-c11
// (and stripped down to the bare minimum).
//
// In theory C++ bitfields are not suited(*1) to replicate an exact
// bit-layout (e.g. of some hardware register or of some binary file-format).
// The C++ standard leaves many details of bitfield layout up to the
// implementation, including things like:
//   * layout from LSB to MSB, or the other way around.
//   * whether fields can cross byte or word boundaries (how large
//     is a single word, whether padding bits are inserted, ...).
//   * ...
// Fortunately in C++, with a relatively simple utility like this, we can take
// full control. Use it like this:
//
// template<size_t POS, size_t WIDTH = 1, typename T2 = bool>
// using BF8 = BitField<uint8_t, POS, WIDTH, T2>;
//
// union MyRegister { // (*2)
//     uint8_t raw = 0;           // the full raw 8 bit value
//     BF8<0> bit0;               // a single bit at bit0
//     BF8<1, 2, uint8_t> bit12;  // a 2-bit field covering bits 1 and 2
//     BF8<3, 5, uint8_t> bit37;  // a 6-bit field covering bits 3 through 7
//     BF8<0, 3, uint8_t> bit012; // a 3-bit field, overlapping with 'bit0' and 'bit12'
// };
// ...
// MyRegister reg;
// ...
// reg.bit0 = true;
// reg.bit12 = 2;
// int a = reg.bit37;
//
// (*1) If you stick to a specific compiler like only gcc, then you can often
//      make it work. But this relies on implementation-specific behavior, and
//      is not guaranteed by the C++ standard.
//
// (*2) Using a union, and then reading/writing fields in arbitrary order may
//      also seem like undefined behavior, normally you're only allowed to read
//      the 'active' member (=last written field). Though here it's fine: all
//      members are trivially layout-compatible and begin with the same
//      underlying type (e.g., uint8_t), satisfying the "common initial
//      sequence" rule.

template<std::unsigned_integral T, size_t POS, size_t WIDTH, typename T2 = T>
	requires (WIDTH > 0 && POS + WIDTH <= std::numeric_limits<T>::digits)
class BitField {
	static constexpr T Mask = (T(1) << WIDTH) - T(1);

public:
	BitField &operator=(T2 newVal) {
		value = T((value & ~(Mask << POS)) | ((newVal & Mask) << POS));
		return *this;
	}

	operator T2() const {
		return (value >> POS) & Mask;
	}

private:
	T value;
};

#endif
