#ifndef STRINGOP_HH
#define STRINGOP_HH

#include "stringsp.hh"
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace StringOp
{
	int stringToInt(const std::string& str);
	bool stringToInt(const std::string& str, int& result);
	unsigned stringToUint(const std::string& str);
	bool stringToUint(const std::string& str, unsigned& result);
	uint64_t stringToUint64(const std::string& str);
	bool stringToBool(std::string_view str);
	double stringToDouble(const std::string& str);
	bool stringToDouble(const std::string& str, double& result);

	std::string toLower(std::string_view str);

	bool startsWith(std::string_view total, std::string_view part);
	bool startsWith(std::string_view total, char part);
	bool endsWith  (std::string_view total, std::string_view part);
	bool endsWith  (std::string_view total, char part);

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

	void splitOnFirst(std::string_view str, std::string_view chars,
	                  std::string_view& first, std::string_view& last);
	void splitOnFirst(std::string_view str, char chars,
	                  std::string_view& first, std::string_view& last);
	void splitOnLast (std::string_view str, std::string_view chars,
	                  std::string_view& first, std::string_view& last);
	void splitOnLast (std::string_view str, char chars,
	                  std::string_view& first, std::string_view& last);
	std::vector<std::string_view> split(std::string_view str, char chars);
	std::vector<unsigned> parseRange(std::string_view str,
	                                 unsigned min, unsigned max);

        unsigned fast_stou(std::string_view s);

	// case insensitive less-than operator
	struct caseless {
		bool operator()(std::string_view s1, std::string_view s2) const {
			auto m = std::min(s1.size(), s2.size());
			int r = strncasecmp(s1.data(), s2.data(), m);
			return (r != 0) ? (r < 0) : (s1.size() < s2.size());
		}
	};
	struct casecmp {
		bool operator()(std::string_view s1, std::string_view s2) const {
			if (s1.size() != s2.size()) return false;
			return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
		}
	};

#if defined(__APPLE__)
	std::string fromCFString(CFStringRef str);
#endif
}

#endif
