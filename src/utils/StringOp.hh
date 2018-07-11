#ifndef STRINGOP_HH
#define STRINGOP_HH

#include "string_view.hh"
#include "stringsp.hh"
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>
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
	bool stringToBool(string_view str);
	double stringToDouble(const std::string& str);
	bool stringToDouble(const std::string& str, double& result);

	std::string toLower(string_view str);

	bool startsWith(string_view total, string_view part);
	bool startsWith(string_view total, char part);
	bool endsWith  (string_view total, string_view part);
	bool endsWith  (string_view total, char part);

	void trimRight(std::string& str, const char* chars);
	void trimRight(std::string& str, char chars);
	void trimRight(string_view& str, string_view chars);
	void trimRight(string_view& str, char chars);
	void trimLeft (std::string& str, const char* chars);
	void trimLeft (std::string& str, char chars);
	void trimLeft (string_view& str, string_view chars);
	void trimLeft (string_view& str, char chars);
	void trim     (string_view& str, string_view chars);
	void trim     (string_view& str, char chars);

	void splitOnFirst(string_view str, string_view chars,
	                  string_view& first, string_view& last);
	void splitOnFirst(string_view str, char chars,
	                  string_view& first, string_view& last);
	void splitOnLast (string_view str, string_view chars,
	                  string_view& first, string_view& last);
	void splitOnLast (string_view str, char chars,
	                  string_view& first, string_view& last);
	std::vector<string_view> split(string_view str, char chars);
	std::string join(const std::vector<string_view>& elems,
	                 char separator);
	std::set<unsigned> parseRange(string_view str,
	                              unsigned min, unsigned max);

	// case insensitive less-than operator
	struct caseless {
		bool operator()(string_view s1, string_view s2) const {
			auto m = std::min(s1.size(), s2.size());
			int r = strncasecmp(s1.data(), s2.data(), m);
			return (r != 0) ? (r < 0) : (s1.size() < s2.size());
		}
	};
	struct casecmp {
		bool operator()(string_view s1, string_view s2) const {
			if (s1.size() != s2.size()) return false;
			return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
		}
	};

#if defined(__APPLE__)
	std::string fromCFString(CFStringRef str);
#endif
}

#endif
