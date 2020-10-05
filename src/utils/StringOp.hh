#ifndef STRINGOP_HH
#define STRINGOP_HH

#include "stringsp.hh"
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace StringOp
{
	[[nodiscard]] int stringToInt(const std::string& str);
	[[nodiscard]] bool stringToInt(const std::string& str, int& result);
	[[nodiscard]] unsigned stringToUint(const std::string& str);
	[[nodiscard]] bool stringToUint(const std::string& str, unsigned& result);
	[[nodiscard]] uint64_t stringToUint64(const std::string& str);
	[[nodiscard]] bool stringToBool(std::string_view str);
	[[nodiscard]] double stringToDouble(const std::string& str);
	[[nodiscard]] bool stringToDouble(const std::string& str, double& result);

	[[nodiscard]] std::string toLower(std::string_view str);

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

	std::pair<std::string_view, std::string_view> splitOnFirst(
		std::string_view str, std::string_view chars);
	std::pair<std::string_view, std::string_view> splitOnFirst(
		std::string_view str, char chars);
	std::pair<std::string_view, std::string_view> splitOnLast(
		std::string_view str, std::string_view chars);
	std::pair<std::string_view, std::string_view> splitOnLast(
		std::string_view str, char chars);
	[[nodiscard]] std::vector<std::string_view> split(std::string_view str, char chars);
	[[nodiscard]] std::vector<unsigned> parseRange(std::string_view str,
	                                               unsigned min, unsigned max);

	[[nodiscard]] unsigned fast_stou(std::string_view s);

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
}

#endif
