#ifndef ESCAPE_NEWLINE_HH
#define ESCAPE_NEWLINE_HH

#include "one_of.hh"

#include <cassert>
#include <cstring>
#include <string>
#include <string_view>

namespace escape_newline {

[[nodiscard]] inline std::string encode(std::string_view input)
{
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
	std::string result;
	result.resize_and_overwrite(input.size(), [&](char* buf, size_t) {
		size_t out = 0;
		for (size_t i = 0; i < input.size(); ++i) {
			char c = input[i];
			if (c == '\\') {
				if (++i == input.size()) break; // incomplete escape, drop
				c = input[i];
				if (c == 'n') c = '\n';
			}
			buf[out++] = c;
		}
		return out;
	});
	return result;
}

inline void decode_inplace(std::string& str)
{
	auto rdPos = str.find('\\');
	if (rdPos == std::string::npos) {
		// No escapes at all, done
		return;
	}

	auto size = str.size();
	auto wrPos = rdPos;
	do {
		assert(rdPos < size);
		assert(str[rdPos] == '\\');
		if (rdPos + 1 == size) {
			// Trailing backslash, shouldn't happen, drop it
			break;
		}

		// Decode escape sequence
		auto c = str[rdPos + 1];
		str[wrPos] = (c == 'n') ? '\n' : c;
		rdPos += 2;
		wrPos += 1;

		// Find next backslash
		auto next_backslash = str.find('\\', rdPos);

		// Copy the intermediate non-backslash chunk
		auto chunk_end = (next_backslash == std::string::npos) ? size : next_backslash;
		auto chunk_len = chunk_end - rdPos;
		memmove(&str[wrPos], &str[rdPos], chunk_len);
		rdPos += chunk_len;
		wrPos += chunk_len;
	} while (rdPos < size);

	str.resize(wrPos);
}

} // namespace escape_newline

#endif
