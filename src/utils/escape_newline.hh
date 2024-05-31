#ifndef ESCAPE_NEWLINE_HH
#define ESCAPE_NEWLINE_HH

#include "one_of.hh"

#include <string>
#include <string_view>

namespace escape_newline {

[[nodiscard]] inline std::string encode(std::string_view input)
{
	// TODO use c++23 std::string::resize_and_overwrite()
	std::string result;
	result.reserve(input.size());
	for (char c : input) {
		if (c == one_of('\n', '\\'))  {
			if (c == '\n') c = 'n';
			result += '\\';
		}
		result += c;
	}
	return result;
}

[[nodiscard]] inline std::string decode(std::string_view input)
{
	// TODO use c++23 std::string::resize_and_overwrite()
	std::string result;
	result.reserve(input.size());
	for (size_t i = 0, end = input.size(); i < end; ++i) {
		char c = input[i];
		if (c == '\\') {
			if (++i == end) break; // error
			c = input[i];
			if (c == 'n') c = '\n';
		}
		result += c;
	}
	return result;
}

} // namespace escape_newline

#endif
