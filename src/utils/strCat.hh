#ifndef STRCAT_HH
#define STRCAT_HH

#include "TemporaryString.hh"
#include "xrange.hh"
#include "zstring_view.hh"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <limits>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

// Suppress false positive -Wstringop-overflow warnings in GCC for template instantiations in this header.
// No push/pop because the warnings come from template instantiations inside this header itself.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

// strCat and strAppend()
//
// Inspired by google's absl::StrCat (similar interface, different implementation).
// See https://abseil.io/blog/20171023-cppcon-strcat
// and https://github.com/abseil/abseil-cpp/blob/master/absl/strings/str_cat.h


// --- Public interface ---

// Concatenate a bunch of 'printable' objects.
//
// Conceptually each object is converted to a string, all those strings are
// concatenated, and that result is returned.
//
// For example:
//      auto s = strCat("foobar", s, ' ', 123);
// is equivalent to
//      auto s = "foobar" + s + ' ' + std::to_string(123);
//
// The former executes faster than the latter because (among other things) it
// immediately creates a result of the correct size instead of (re)allocating
// all intermediate strings. Also it doesn't create temporary string objects
// for the integer conversions.
template<typename... Ts>
[[nodiscard]] std::string strCat(Ts&& ...ts);

// Consider using 'tmpStrCat()' as an alternative for 'strCat()'. The only
// difference is that this one returns a 'TemporaryString' instead of a
// 'std::string'. This can be faster (e.g. no heap allocation) when the result
// is not required to be a 'std::string' (std::string_view is sufficient) and
// when it really is a temporary (short lived) string.
template<typename... Ts>
[[nodiscard]] TemporaryString tmpStrCat(Ts&&... ts);

// Append a bunch of 'printable' objects to an exiting string.
//
// Can be used to optimize
//     s += strCat(a, b, c);
// to
//     strAppend(s, a, b, c);
//
// This latter will immediately construct the result in 's' instead of first
// formatting to a temporary string followed by appending that temporary to 's'.
//
// There's one limitation though: it's not allowed to append a string to
// itself, not even parts of the string (strCat() doesn't have this
// limitation). So the following is invalid:
//     strAppend(s, s); // INVALID, append s to itself
//
//     string_view v = s;
//     v.substr(10, 20);
//     strAppend(s, v); // INVALID, append part of s to itself
template<typename... Ts>
void strAppend(std::string& result, Ts&& ...ts);


// Format an integer as a fixed-width hexadecimal value and insert it into
// a strCat() or strAppend() sequence.
// - If the value is small, leading zeros are printed.
// - If the value is too big, it gets truncated. Only the rightmost characters
//   are kept.
//
// For example:
//    s = strCat("The value is: 0x", hex_string<4>(value), '.');
////template<size_t N, typename T>
////strCatImpl::ConcatFixedWidthHexIntegral<N, T> hex_string(T t);


// Insert 'n' spaces into a strCat() or strAppend() sequence.
//
// For example:
//    s = strCat("The result is ", spaces(30 - item.size()), item);
//
// This can be more efficient than creating a temporary string containing 'n'
// spaces, like this
//    s = strCat("The result is ", std::string(30 - item.size(), ' '), item);
////strCatImpl::ConcatSpaces spaces(size_t t);

struct Digits {
	size_t n;
};
enum class HexCase : uint8_t {
	lower, upper
};

// --- Implementation details ---

namespace strCatImpl {

// ConcatUnit
// These implement various mechanisms to concatenate an object to a string.
// All these classes implement:
//
// - size_t size() const;
//     Returns the (exact) size in characters of the formatted object.
//
// - char* copy(char* dst) const;
//     Copy the formatted object to 'dst', returns an updated pointer.
template<typename T> struct ConcatUnit;


// Helper for types which are formatted via a temporary string object
struct ConcatViaString
{
	explicit ConcatViaString(std::string s_)
		: s(std::move(s_))
	{
	}

	[[nodiscard]] size_t size() const
	{
		return s.size();
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		std::ranges::copy(s, dst);
		return dst + s.size();
	}

private:
	std::string s;
};

// Helper for types which are printed via std::to_string(),
// e.g. floating point types.
template<typename T>
struct ConcatToString : ConcatViaString
{
	explicit ConcatToString(T t)
		: ConcatViaString(std::to_string(t))
	{
	}
};

// The default (slow) implementation uses 'operator<<(ostream&, T)'
template<typename T>
struct ConcatUnit : ConcatViaString
{
	explicit ConcatUnit(const T& t)
		: ConcatViaString([&]{
			std::ostringstream os;
			os << t;
			return os.str();
		}())
	{
	}
};


// ConcatUnit<string_view>:
//   store the string view (copies the view, not the string)
template<> struct ConcatUnit<std::string_view>
{
	explicit ConcatUnit(const std::string_view v_)
		: v(v_)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return v.size();
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		std::ranges::copy(v, dst);
		return dst + v.size();
	}

private:
	std::string_view v;
};


// ConcatUnit<char>:
//   store single char (length is constant 1)
template<> struct ConcatUnit<char>
{
	explicit ConcatUnit(char c_)
		: c(c_)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return 1;
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		*dst = c;
		return dst + 1;
	}

private:
	char c;
};


// ConcatUnit<bool>:
//   store bool (length is constant 1)
template<> struct ConcatUnit<bool>
{
	explicit ConcatUnit(bool b_)
		: b(b_)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return 1;
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		*dst = b ? '1' : '0';
		return dst + 1;
	}

private:
	bool b;
};


// Helper classes/functions for fast integer -> string conversion.

// Returns a fast type that is wide enough to hold the absolute value for
// values of the given type.
//
// On x86 32-bit operations are faster than e.g. 16-bit operations.
// So this function returns a 32-bit type unless 64-bit are needed.
//
// 'long' is special because it's either 32-bit (windows) or 64-bit (Linux).
template<typename T> struct FastUnsignedImpl { using type = unsigned; };
template<> struct FastUnsignedImpl<              long> { using type = unsigned long;      };
template<> struct FastUnsignedImpl<unsigned      long> { using type = unsigned long;      };
template<> struct FastUnsignedImpl<         long long> { using type = unsigned long long; };
template<> struct FastUnsignedImpl<unsigned long long> { using type = unsigned long long; };
template<typename T> using FastUnsigned = typename FastUnsignedImpl<T>::type;

// Helper function to take the absolute value of a signed or unsigned type.
//  (without compiler warning on 't < 0' and '-t' when t is unsigned)
template<std::unsigned_integral T>
[[nodiscard]] FastUnsigned<T> absHelper(T t) { return t; }

template<std::signed_integral T>
[[nodiscard]] FastUnsigned<T> absHelper(T t) { return (t < 0)? -t : t; }


// Optimized integer printing.
//
// Prints the value to an internal buffer. The formatted characters are
// generated from back to front. This means the final result is not aligned
// with the start of this internal buffer.
//
// Next to the internal buffer we also store the size (in characters) of the
// result. This size can be used to calculate the start position in the buffer.
template<std::integral T> struct ConcatIntegral
{
	static constexpr bool IS_SIGNED = std::numeric_limits<T>::is_signed;
	static constexpr size_t BUF_SIZE = 1 + std::numeric_limits<T>::digits10 + IS_SIGNED;

	explicit ConcatIntegral(T t)
	{
		auto p = this->end();
		std::unsigned_integral auto a = absHelper(t);

		do {
			*--p = static_cast<char>('0' + (a % 10));
			a /= 10;
		} while (a);

		if constexpr (IS_SIGNED) {
			if (t < 0) *--p = '-';
		}
		this->sz = static_cast<unsigned char>(this->end() - p);
	}

	[[nodiscard]] size_t size() const
	{
		return sz;
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		std::ranges::copy(std::span{data(), sz}, dst);
		return dst + sz;
	}

	[[nodiscard]] char* copyTail(char* dst, size_t n) const
	{
		assert(n <= sz);
		std::ranges::copy(std::span{buf.data() + BUF_SIZE - n, n}, dst);
		return dst + n;
	}

	[[nodiscard]] operator std::string() const
	{
		return std::string(data(), this->size());
	}

private:
	[[nodiscard]] const char* data() const { return buf.data() + BUF_SIZE - sz; }
	[[nodiscard]] auto end() { return buf.end(); }

private:
	std::array<char, BUF_SIZE> buf;
	unsigned char sz;
};


// Format an integral as a hexadecimal value with a fixed number of characters.
// This fixed width means it either adds leading zeros or truncates the result
// (it keeps the rightmost digits).
template<HexCase Case, std::integral T> struct ConcatVariableWidthHexIntegral
{
	ConcatVariableWidthHexIntegral(Digits n_, T t_)
		: n(n_.n), t(t_)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return n;
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		char* p = dst + n;
		auto u = static_cast<FastUnsigned<T>>(t);

		static constexpr char A = (Case == HexCase::lower) ? 'a' : 'A';
		repeat(n, [&] {
			auto d = u & 15;
			*--p = (d < 10) ? static_cast<char>(d + '0')
			                : static_cast<char>(d - 10 + A);
			u >>= 4;
		});

		return dst + n;
	}

private:
	size_t n;
	T t;
};

// Format an integral as a hexadecimal value with a fixed number of characters.
// This fixed width means it either adds leading zeros or truncates the result
// (it keeps the rightmost digits).
template<size_t N, HexCase Case, std::integral T> struct ConcatFixedWidthHexIntegral
{
	explicit ConcatFixedWidthHexIntegral(T t_)
		: t(t_)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return N;
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		char* p = dst + N;
		auto u = static_cast<FastUnsigned<T>>(t);

		static constexpr char A = (Case == HexCase::lower) ? 'a' : 'A';
		repeat(N, [&] {
			auto d = u & 15;
			*--p = (d < 10) ? static_cast<char>(d + '0')
			                : static_cast<char>(d - 10 + A);
			u >>= 4;
		});

		return dst + N;
	}

private:
	T t;
};

// Format an integral as a binary value with a fixed number of characters.
// This fixed width means it either adds leading zeros or truncates the result
// (it keeps the rightmost digits).
template<size_t N, std::integral T> struct ConcatFixedWidthBinIntegral
{
	explicit ConcatFixedWidthBinIntegral(T t_)
		: t(t_)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return N;
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		char* p = dst + N;
		auto u = static_cast<FastUnsigned<T>>(t);

		repeat(N, [&] {
			*--p = static_cast<char>((u & 1) + '0');
			u >>= 1;
		});

		return dst + N;
	}

private:
	T t;
};


// Prints a number of spaces (without constructing a temporary string).
struct ConcatSpaces
{
	explicit ConcatSpaces(size_t n_)
		: n(n_)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return n;
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		std::ranges::fill(std::span{dst, n}, ' ');
		return dst + n;
	}

private:
	size_t n;
};


// Format an integral as a decimal value with a fixed number of characters.
// This fixed width means it either adds leading spaces or truncates the result
// (it keeps the rightmost digits).
template<size_t N, std::integral T> struct ConcatFixedWidthDecIntegral
{
	explicit ConcatFixedWidthDecIntegral(T t)
		: helper(t)
	{
	}

	[[nodiscard]] size_t size() const
	{
		return N;
	}

	[[nodiscard]] char* copy(char* dst) const
	{
		auto n2 = helper.size();
		if (N <= n2) {
			return helper.copyTail(dst, N);
		} else {
			ConcatSpaces spaces(N - n2);
			auto* p = spaces.copy(dst);
			return helper.copy(p);
		}
	}

private:
	ConcatIntegral<T> helper;
};


// Create a 'ConcatUnit<T>' wrapper object for a given 'T' value.

// Generic version: use the corresponding ConcatUnit<T> class. This can be
// a specialized version for 'T', or the generic (slow) version which uses
// operator<<(ostream&, T).
template<typename T>
[[nodiscard]] inline auto makeConcatUnit(const T& t)
{
	return ConcatUnit<T>(t);
}

// Overloads for various cases (strings, integers, floats, ...).
[[nodiscard]] inline auto makeConcatUnit(const std::string& s)
{
	return ConcatUnit<std::string_view>(s);
}

[[nodiscard]] inline auto makeConcatUnit(const char* s)
{
	return ConcatUnit<std::string_view>(s);
}

[[nodiscard]] inline auto makeConcatUnit(char* s)
{
	return ConcatUnit<std::string_view>(s);
}
[[nodiscard]] inline auto makeConcatUnit(const TemporaryString& s)
{
	return ConcatUnit<std::string_view>(s);
}
[[nodiscard]] inline auto makeConcatUnit(zstring_view s)
{
	return ConcatUnit<std::string_view>(s);
}

// Note: no ConcatIntegral<char> because that is printed as a single character
[[nodiscard]] inline auto makeConcatUnit(signed char c)
{
	return ConcatIntegral<signed char>(c);
}

[[nodiscard]] inline auto makeConcatUnit(unsigned char c)
{
	return ConcatIntegral<unsigned char>(c);
}

[[nodiscard]] inline auto makeConcatUnit(short s)
{
	return ConcatIntegral<short>(s);
}

[[nodiscard]] inline auto makeConcatUnit(unsigned short s)
{
	return ConcatIntegral<unsigned short>(s);
}

[[nodiscard]] inline auto makeConcatUnit(int i)
{
	return ConcatIntegral<int>(i);
}

[[nodiscard]] inline auto makeConcatUnit(unsigned u)
{
	return ConcatIntegral<unsigned>(u);
}

[[nodiscard]] inline auto makeConcatUnit(long l)
{
	return ConcatIntegral<long>(l);
}

[[nodiscard]] inline auto makeConcatUnit(unsigned long l)
{
	return ConcatIntegral<unsigned long>(l);
}

[[nodiscard]] inline auto makeConcatUnit(long long l)
{
	return ConcatIntegral<long long>(l);
}

[[nodiscard]] inline auto makeConcatUnit(unsigned long long l)
{
	return ConcatIntegral<unsigned long long>(l);
}

// Converting float->string via std::to_string() might be faster than via
// std::stringstream.

[[nodiscard]] inline auto makeConcatUnit(float f)
{
	return ConcatToString<float>(f);
}

[[nodiscard]] inline auto makeConcatUnit(double d)
{
	return ConcatToString<double>(d);
}

[[nodiscard]] inline auto makeConcatUnit(long double d)
{
	return ConcatToString<long double>(d);
}

template<HexCase Case, std::integral T>
[[nodiscard]] inline auto makeConcatUnit(const ConcatVariableWidthHexIntegral<Case, T>& t)
{
	return t;
}

template<size_t N, HexCase Case, std::integral T>
[[nodiscard]] inline auto makeConcatUnit(const ConcatFixedWidthHexIntegral<N, Case, T>& t)
{
	return t;
}

template<size_t N, std::integral T>
[[nodiscard]] inline auto makeConcatUnit(const ConcatFixedWidthBinIntegral<N, T>& t)
{
	return t;
}

template<size_t N, std::integral T>
[[nodiscard]] inline auto makeConcatUnit(const ConcatFixedWidthDecIntegral<N, T>& t)
{
	return t;
}

[[nodiscard]] inline auto makeConcatUnit(const ConcatSpaces& t)
{
	return t;
}


// Calculate the total size for a bunch (a tuple) of ConcatUnit<T> objects.
// That is, calculate the sum of calling the size() method on each ConcatUnit.
template<typename Tuple, size_t... Is>
[[nodiscard]] size_t calcTotalSizeHelper(const Tuple& t, std::index_sequence<Is...>)
{
	return (... + std::get<Is>(t).size());
}

template<typename... Ts>
[[nodiscard]] size_t calcTotalSize(const std::tuple<Ts...>& t)
{
	return calcTotalSizeHelper(t, std::index_sequence_for<Ts...>{});
}


// Copy each ConcatUnit<T> in the given tuple to the final result.
template<typename Tuple, size_t... Is>
void copyUnitsHelper(char* dst, const Tuple& t, std::index_sequence<Is...>)
{
	auto l = { ((dst = std::get<Is>(t).copy(dst)) , 0)... };
	(void)l;
}

template<typename... Ts>
void copyUnits(char* dst, const std::tuple<Ts...>& t)
{
	copyUnitsHelper(dst, t, std::index_sequence_for<Ts...>{});
}

// Fast integral -> string conversion. (Standalone version, result is not part
// of a larger string).
[[nodiscard]] inline std::string to_string(std::integral auto x)
{
	return ConcatIntegral(x);
}

} // namespace strCatImpl


// Generic version
template<typename... Ts>
[[nodiscard]] std::string strCat(Ts&& ...ts)
{
	// Strategy:
	// - For each parameter (of any type) we create a ConcatUnit object.
	// - We sum the results of callings size() on all those objects.
	// - We allocate a string of that total size.
	// - We copy() each ConcatUnit into that string.

	auto t = std::tuple(strCatImpl::makeConcatUnit(std::forward<Ts>(ts))...);
	auto size = strCatImpl::calcTotalSize(t);

	std::string result;
	result.resize_and_overwrite(size, [&](char* dst, size_t sz) {
		strCatImpl::copyUnits(dst, t);
		return sz;
	});
	return result;
}

// Optimized strCat() for the case that the 1st parameter is a temporary
// (rvalue) string. In that case we can append to that temporary.
template<typename... Ts>
[[nodiscard]] std::string strCat(std::string&& first, Ts&& ...ts)
{
	strAppend(first, std::forward<Ts>(ts)...);
	return std::move(first);
}

// Degenerate case
[[nodiscard]] inline std::string strCat()
{
	return {};
}

// Extra overloads. These don't change and/or extend the above functionality,
// but in some cases they might improve performance a bit (see notes above
// about uninitialized string resize). With these overloads strCat()/strAppend()
// should never be less efficient than a sequence of + or += string-operations.
[[nodiscard]] inline std::string strCat(const std::string& x) { return x; }
[[nodiscard]] inline std::string strCat(std::string&&      x) { return std::move(x); }
[[nodiscard]] inline std::string strCat(const char*        x) { return {x}; }
[[nodiscard]] inline std::string strCat(char               x) { return std::string(1, x); } // Not: return {1, x};
[[nodiscard]] inline std::string strCat(std::string_view   x) { return {x.data(), x.size()}; }

[[nodiscard]] inline std::string strCat(signed char        x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(unsigned char      x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(short              x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(unsigned short     x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(int                x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(unsigned           x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(long               x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(unsigned long      x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(long long          x) { return strCatImpl::to_string(x); }
[[nodiscard]] inline std::string strCat(unsigned long long x) { return strCatImpl::to_string(x); }

[[nodiscard]] inline std::string strCat(const std::string& x, const std::string& y) { return x + y; }
[[nodiscard]] inline std::string strCat(const char*        x, const std::string& y) { return x + y; }
[[nodiscard]] inline std::string strCat(char               x, const std::string& y) { return x + y; }
[[nodiscard]] inline std::string strCat(const std::string& x, const char*        y) { return x + y; }
[[nodiscard]] inline std::string strCat(const std::string& x, char               y) { return x + y; }
[[nodiscard]] inline std::string strCat(std::string&&      x, const std::string& y) { return std::move(x) + y; }
[[nodiscard]] inline std::string strCat(const std::string& x, std::string&&      y) { return x + std::move(y); }
[[nodiscard]] inline std::string strCat(std::string&&      x, std::string&&      y) { return std::move(x) + std::move(y); }
[[nodiscard]] inline std::string strCat(const char*        x, std::string&&      y) { return x + std::move(y); }
[[nodiscard]] inline std::string strCat(char               x, std::string&&      y) { return x + std::move(y); }
[[nodiscard]] inline std::string strCat(std::string&&      x, const char*        y) { return std::move(x) + y; }
[[nodiscard]] inline std::string strCat(std::string&&      x, char               y) { return std::move(x) + y; }

template<typename... Ts> [[nodiscard]] TemporaryString tmpStrCat(Ts&&... ts)
{
	auto t = std::tuple(strCatImpl::makeConcatUnit(std::forward<Ts>(ts))...);
	auto size = strCatImpl::calcTotalSize(t);
	return TemporaryString(
	        size, [&](char* dst) { strCatImpl::copyUnits(dst, t); });
}

// Generic version
template<typename... Ts>
void strAppend(std::string& result, Ts&& ...ts)
{
	// Implementation strategy is similar to strCat(). Main difference is
	// that we now extend an existing string instead of creating a new one.

	auto t = std::tuple(strCatImpl::makeConcatUnit(std::forward<Ts>(ts))...);
	auto extraSize = strCatImpl::calcTotalSize(t);
	auto oldSize = result.size();

	result.resize_and_overwrite(oldSize + extraSize, [&](char* p, size_t sz) {
		// p[0..oldSize-1] still/already contains old content
		strCatImpl::copyUnits(&p[oldSize], t);
		return sz;
	});
}

// Degenerate case
inline void strAppend(std::string& /*x*/)
{
	// nothing
}

// Extra overloads, see strCat().
inline void strAppend(std::string& x, const std::string& y) { x += y; }
inline void strAppend(std::string& x, const char*        y) { x += y; }
inline void strAppend(std::string& x, std::string_view   y) { x.append(y.data(), y.size()); }


template<HexCase Case = HexCase::lower, std::integral T>
[[nodiscard]] inline strCatImpl::ConcatVariableWidthHexIntegral<Case, T> hex_string(Digits n, T t)
{
	return {n, t};
}

template<size_t N, HexCase Case = HexCase::lower, std::integral T>
[[nodiscard]] inline auto hex_string(T t)
{
	return strCatImpl::ConcatFixedWidthHexIntegral<N, Case, T>{t};
}

template<size_t N, std::integral T>
[[nodiscard]] inline auto bin_string(T t)
{
	return strCatImpl::ConcatFixedWidthBinIntegral<N, T>{t};
}

template<size_t N, std::integral T>
[[nodiscard]] inline auto dec_string(T t)
{
	return strCatImpl::ConcatFixedWidthDecIntegral<N, T>{t};
}

[[nodiscard]] inline auto spaces(size_t n)
{
	return strCatImpl::ConcatSpaces{n};
}

#endif
