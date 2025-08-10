#ifndef STRINGOP_HH
#define STRINGOP_HH

#include "IterableBitSet.hh"
#include "stringsp.hh"

#include <algorithm>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifdef __APPLE__
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
	template<std::integral T> [[nodiscard]] std::optional<T> stringTo(std::string_view s);

	/** As above, but without dynamic base detection. Moreover leading
	  * prefixes like '0x' for hexadecimal are seen as invalid input.
	  */
	template<int BASE, std::integral T> [[nodiscard]] std::optional<T> stringToBase(std::string_view s);

	[[nodiscard]] bool stringToBool(std::string_view str);

	[[nodiscard]] std::string toLower(std::string_view str);

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

	enum class EmptyParts : uint8_t {
		KEEP,  // "a,b,,c" -> "a", "b", "", "c"
		REMOVE // "a,b,,c" -> "a", "b", "c"       BUT  ",,a,b" -> "", "a", "b"  (keeps one empty part in front)
	};
	template<EmptyParts keepOrRemove = EmptyParts::KEEP, typename Separators>
	[[nodiscard]] inline auto split_view(std::string_view str, Separators separators) {
		struct Iterator {
			using value_type      = std::string_view;
			using difference_type = ptrdiff_t;

			Iterator() = default;
			Iterator(const Iterator&) = default;
			Iterator& operator=(const Iterator&) = default;

			Iterator(std::string_view str_, Separators separators_)
				: str(str_), separators(separators_) {
				str.remove_prefix(skipSeparators(0));
				next_p();
			}

			[[nodiscard]] value_type operator*() const {
				difference_type dummy; (void)dummy; // avoid warning about unused typedef
				return {str.data(), p};
			}

			Iterator& operator++() {
				if (p < str.size()) {
					str.remove_prefix(skipSeparators(p + 1));
					next_p();
				} else {
					str = "";
				}
				return *this;
			}
			Iterator operator++(int) { auto copy = *this; ++(*this); return copy; }

			[[nodiscard]] bool operator==(const Iterator&) const = default;
			[[nodiscard]] bool operator==(std::default_sentinel_t) const { return str.empty(); }

		private:
			static bool isSeparator(char c, char separators) {
				return c == separators;
			}
			static bool isSeparator(char c, std::string_view separators) {
				return separators.contains(c);
			}

			void next_p() {
				p = 0;
				while ((p < str.size()) && !isSeparator(str[p], separators)) ++p;
			}

			[[nodiscard]] std::string_view::size_type skipSeparators(std::string_view::size_type pos) const {
				if (keepOrRemove == EmptyParts::REMOVE) {
					while ((pos < str.size()) && isSeparator(str[pos], separators)) ++pos;
				}
				return pos;
			}

		private:
			std::string_view str;
			std::string_view::size_type p;
			Separators separators;
		};
		static_assert(std::forward_iterator<Iterator>);
		static_assert(std::sentinel_for<std::default_sentinel_t, Iterator>);

		struct Splitter {
			std::string_view str;
			Separators separators;

			[[nodiscard]] auto begin() const { return Iterator{str, separators}; }
			[[nodiscard]] auto end()   const { return std::default_sentinel_t{}; }
		};

		return Splitter{str, separators};
	}

	// case insensitive less-than operator
	struct caseless {
		[[nodiscard]] bool operator()(std::string_view s1, std::string_view s2) const {
			auto m = std::min(s1.size(), s2.size());
			int r = strncasecmp(s1.data(), s2.data(), m);
			return (r != 0) ? (r < 0) : (s1.size() < s2.size());
		}
	};
	// case insensitive greater-than operator
	struct inv_caseless {
		[[nodiscard]] bool operator()(std::string_view s1, std::string_view s2) const {
			auto m = std::min(s1.size(), s2.size());
			int r = strncasecmp(s1.data(), s2.data(), m);
			return (r != 0) ? (r > 0) : (s1.size() > s2.size());
		}
	};
	struct casecmp {
		[[nodiscard]] bool operator()(std::string_view s1, std::string_view s2) const {
			if (s1.size() != s2.size()) return false;
			return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
		}
	};

	[[nodiscard]] inline bool containsCaseInsensitive(std::string_view haystack, std::string_view needle)
	{
		return !std::ranges::search(haystack, needle,
		                   [](char x, char y) { return toupper(x) == toupper(y); }
		        ).empty();
	}

#ifdef __APPLE__
	[[nodiscard]] std::string fromCFString(CFStringRef str);
#endif

	template<int BASE, std::integral T>
	[[nodiscard]] std::optional<T> stringToBase(std::string_view s)
	{
		T result = {}; // dummy init to avoid warning
		const auto* b = s.data();
		const auto* e = s.data() + s.size();
		if (auto [p, ec] = std::from_chars(b, e, result, BASE);
		    (ec == std::errc()) && (p == e)) {
			return result;
		}
		return std::nullopt;
	}

	template<std::integral T>
	[[nodiscard]] std::optional<T> stringTo(std::string_view s)
	{
		if (s.empty()) [[unlikely]] return {};
		if constexpr (std::is_signed_v<T>) {
			bool negate = false;
			if (s[0] == '-') [[unlikely]] {
				negate = true;
				s.remove_prefix(1);
			}

			using U = std::make_unsigned_t<T>;
			auto tmp = stringTo<U>(s);
			if (!tmp) [[unlikely]] return {};

			U max = U(std::numeric_limits<T>::max()) + 1; // 0x8000
			if (negate) [[unlikely]] {
				if (*tmp > max) [[unlikely]] return {}; // 0x8000
				return T(~*tmp + 1);
			} else {
				if (*tmp >= max) [[unlikely]] return {}; // 0x7fff
				return T(*tmp);
			}
		} else {
			if (s[0] != '0') [[likely]] {
				return stringToBase<10, T>(s);
			} else {
				if (s.size() == 1) return T(0);
				if ((s[1] == 'x') || (s[1] == 'X')) [[likely]] {
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
