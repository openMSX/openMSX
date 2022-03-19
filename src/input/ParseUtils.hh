#ifndef PARSEUTILS_HH
#define PARSEUTILS_HH

#include "one_of.hh"

#include <string_view>
#include <utility>

namespace openmsx {

/* Remove the next line from 'text' and return it.
 * A line is everything upto the first newline character. The character is
 * removed from 'text' but not included in the return value.
 */
[[nodiscard]] inline constexpr std::string_view getLine(std::string_view& text)
{
	if (auto pos = text.find_first_of('\n'); pos != std::string_view::npos) {
		// handle both 'LF' and 'CR LF'
		auto pos2 = ((pos != 0) && (text[pos - 1] == '\r')) ? pos - 1 : pos;
		auto result = text.substr(0, pos2);
		text.remove_prefix(pos + 1);
		return result;
	}
	return std::exchange(text, {});
}

/* Return the given line with comments at the end of the line removed.
 * Comments start at the first '#' character and continue till the end of the
 * line.
 */
[[nodiscard]] inline constexpr std::string_view stripComments(std::string_view line)
{
	if (auto pos = line.find_first_of('#'); pos != std::string_view::npos) {
		line = line.substr(0, pos);
	}
	return line;
}

/* Returns true iff the given character is a separator (whitespace).
 * Newline and hash-mark are handled (already removed) by other functions.
 */
template<typename ...Separators>
[[nodiscard]] inline constexpr bool isSep(char c, Separators... separators)
{
	return c == one_of(' ', '\t', '\r', separators...); // whitespace
}

/* Remove one token from 'line' and return it.
 * Tokens are separated by one or more separator character as defined by 'isSep()'.
 * Those separators are removed from 'line' but not included in the return value.
 * The assumption is that there are no leading separator characters before calling this function.
 */
template<typename ...Separators>
inline constexpr std::string_view getToken(std::string_view& line, Separators... separators)
{
	size_t s = line.size();
	size_t i = 0;
	while ((i < s) && !isSep(line[i], separators...)) {
		++i;
	}
	auto result = line.substr(0, i);
	while ((i < s) && isSep(line[i], separators...)) {
		++i;
	}
	line.remove_prefix(i);
	return result;
}

} // namespace openmsx

#endif
