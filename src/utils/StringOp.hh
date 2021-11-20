#ifndef STRINGOP_HH
#define STRINGOP_HH

#include "IterableBitSet.hh"
#include "likely.hh"
#include "stringsp.hh"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace StringOp
{
	/** Convert a string to an integral type 'T' (int, uint64_t, ...).
	  * This is similar to, but not quite the same as the family of
	  * 'strtoll()' functions:
	  * - Leading whitespace is NOT accepted (unlike strtoll()).
	  * - There may NOT be any trailing character after the value (unlike
	  *   strtoll()).
	  * - (Only) if 'T' is a signed type, the value may start with a '-'
	  *   character.
	  * - A leading '+' character is NOT accepted.
	  * - It's an error if the value cannot be represented by the type 'T'.
	  * - This function tries to detect the base of the value by looking at
	  *   the prefix. (Only) these prefixes are accepted:
	  *    - '0x' or '0X': hexadecimal
	  *    - '0b' or '0B': binary
	  *    - no prefix: decimal
	  *   Note that prefix '0' for octal is NOT supported.
	  * - The input is a 'string_view' (rather than a 'const char*'), so it
	  *   is not required to be zero-terminated.
	  */
	template<typename T> [[nodiscard]] std::optional<T> stringTo(std::string_view s);

	/** As above, but without dynamic base detection. Moreover leading
	  * prefixes like '0x' for hexadecimal are seen as invalid input.
	  */
	template<int BASE, typename T> [[nodiscard]] std::optional<T> stringToBase(std::string_view s);

	[[nodiscard]] bool stringToBool(std::string_view str);

	//[[nodiscard]] std::string toLower(std::string_view str);

	[[nodiscard]] bool startsWith(std::string_view total, std::string_view part);
	[[nodiscard]] bool startsWith(std::string_view total, char part);
	[[nodiscard]] bool endsWith  (std::string_view total, std::string_view part);
	[[nodiscard]] bool endsWith  (std::string_view total, char part);

	void trimRight(std::string& str, const char* chars);
	void trimRight(std::string& str, char chars);
	void trimRight(std::string_view& str, std::string_view chars);
	void trimRight(std::string_view& str, char chars);
	void trimLeft (std::string& str, const char* chars);
	void trimLeft (std::string& str, char chars);
	void trimLeft (std::string_view& str, std::string_view chars);
	void trimLeft (std::string_view& str, char chars);
	void trim     (std::string_view& str, std::string_view chars);
	void trim     (std::string_view& str, char chars);

	[[nodiscard]] std::pair<std::string_view, std::string_view> splitOnFirst(
		std::string_view str, std::string_view chars);
	[[nodiscard]] std::pair<std::string_view, std::string_view> splitOnFirst(
		std::string_view str, char chars);
	[[nodiscard]] std::pair<std::string_view, std::string_view> splitOnLast(
		std::string_view str, std::string_view chars);
	[[nodiscard]] std::pair<std::string_view, std::string_view> splitOnLast(
		std::string_view str, char chars);
	[[nodiscard]] IterableBitSet<64> parseRange(std::string_view str,
	                                            unsigned min, unsigned max);

	//[[nodiscard]] std::vector<std::string_view> split(std::string_view str, char chars);

	[[nodiscard]] inline auto split_view(std::string_view str, char c) {
		struct Sentinel {};

		struct Iterator {
			std::string_view str;
			std::string_view::size_type p;
			const char c;

			Iterator(std::string_view str_, char c_)
				: str(str_), c(c_) {
				next_p();
			}

			[[nodiscard]] std::string_view operator*() const {
				return std::string_view(str.data(), p);
			}

			Iterator& operator++() {
				if (p < str.size()) {
					str.remove_prefix(p + 1);
					next_p();
				} else {
					str = "";
				}
				return *this;
			}

			[[nodiscard]] bool operator==(Sentinel) const { return  str.empty(); }
			[[nodiscard]] bool operator!=(Sentinel) const { return !str.empty(); }

			void next_p() {
				p = 0;
				while ((p < str.size()) && (str[p] != c)) ++p;
			}
		};

		struct Splitter {
			std::string_view str;
			char c;

			[[nodiscard]] auto begin() const { return Iterator{str, c}; }
			[[nodiscard]] auto end()   const { return Sentinel{}; }
		};

		return Splitter{str, c};
	}

	// case insensitive less-than operator
	struct caseless {
		[[nodiscard]] bool operator()(std::string_view s1, std::string_view s2) const {
			auto m = std::min(s1.size(), s2.size());
			int r = strncasecmp(s1.data(), s2.data(), m);
			return (r != 0) ? (r < 0) : (s1.size() < s2.size());
		}
	};
	struct casecmp {
		[[nodiscard]] bool operator()(std::string_view s1, std::string_view s2) const {
			if (s1.size() != s2.size()) return false;
			return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
		}
	};

#if defined(__APPLE__)
	[[nodiscard]] std::string fromCFString(CFStringRef str);
#endif

	template<int BASE, typename T>
	[[nodiscard]] std::optional<T> stringToBase(std::string_view s)
	{
		T result;
		auto b = s.data();
		auto e = s.data() + s.size();
		if (auto [p, ec] = std::from_chars(b, e, result, BASE);
		    (ec == std::errc()) && (p == e)) {
			return result;
		}
		return std::nullopt;
	}

	template<typename T>
	[[nodiscard]] std::optional<T> stringTo(std::string_view s)
	{
		if (unlikely(s.empty())) return {};
		if constexpr (std::is_signed_v<T>) {
			bool negate = false;
			if (unlikely(s[0] == '-')) {
				negate = true;
				s.remove_prefix(1);
			}

			using U = std::make_unsigned_t<T>;
			auto tmp = stringTo<U>(s);
			if (unlikely(!tmp)) return {};

			U max = U(std::numeric_limits<T>::max()) + 1; // 0x8000
			if (unlikely(negate)) {
				if (unlikely(*tmp > max)) return {}; // 0x8000
				return T(~*tmp + 1);
			} else {
				if (unlikely(*tmp >= max)) return {}; // 0x7fff
				return T(*tmp);
			}
		} else {
			if (likely(s[0] != '0')) {
				return stringToBase<10, T>(s);
			} else {
				if (s.size() == 1) return T(0);
				if (likely((s[1] == 'x') || (s[1] == 'X'))) {
					s.remove_prefix(2);
					return stringToBase<16, T>(s);
				} else if ((s[1] == 'b') || (s[1] == 'B')) {
					s.remove_prefix(2);
					return stringToBase<2, T>(s);
				} else {
					return stringToBase<10, T>(s);
				}
			}
		}
	}
}

#endif
