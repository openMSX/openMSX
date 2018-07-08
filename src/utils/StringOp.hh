#ifndef STRINGOP_HH
#define STRINGOP_HH

#include "string_ref.hh"
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
	bool stringToBool(string_ref str);
	double stringToDouble(const std::string& str);
	bool stringToDouble(const std::string& str, double& result);

	std::string toLower(string_ref str);

	bool startsWith(string_ref total, string_ref part);
	bool startsWith(string_ref total, char part);
	bool endsWith  (string_ref total, string_ref part);
	bool endsWith  (string_ref total, char part);

	void trimRight(std::string& str, const char* chars);
	void trimRight(std::string& str, char chars);
	void trimRight(string_ref& str, string_ref chars);
	void trimRight(string_ref& str, char chars);
	void trimLeft (std::string& str, const char* chars);
	void trimLeft (std::string& str, char chars);
	void trimLeft (string_ref& str, string_ref chars);
	void trimLeft (string_ref& str, char chars);
	void trim     (string_ref& str, string_ref chars);
	void trim     (string_ref& str, char chars);

	void splitOnFirst(string_ref str, string_ref chars,
	                  string_ref& first, string_ref& last);
	void splitOnFirst(string_ref str, char chars,
	                  string_ref& first, string_ref& last);
	void splitOnLast (string_ref str, string_ref chars,
	                  string_ref& first, string_ref& last);
	void splitOnLast (string_ref str, char chars,
	                  string_ref& first, string_ref& last);
	std::vector<string_ref> split(string_ref str, char chars);
	std::string join(const std::vector<string_ref>& elems,
	                 char separator);
	std::set<unsigned> parseRange(string_ref str,
	                              unsigned min, unsigned max);

	// case insensitive less-than operator
	struct caseless {
		bool operator()(string_ref s1, string_ref s2) const {
			auto m = std::min(s1.size(), s2.size());
			int r = strncasecmp(s1.data(), s2.data(), m);
			return (r != 0) ? (r < 0) : (s1.size() < s2.size());
		}
	};
	struct casecmp {
		bool operator()(string_ref s1, string_ref s2) const {
			if (s1.size() != s2.size()) return false;
			return strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
		}
	};

#if defined(__APPLE__)
	std::string fromCFString(CFStringRef str);
#endif
}

#endif
