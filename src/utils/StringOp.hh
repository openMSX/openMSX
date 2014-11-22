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
	class Builder
	{
	public:
		Builder();
		~Builder();

		// Overloaded for the most common types, to avoid having to use
		// the templatized version below (expect for being implicitly
		// inlined, the template is just fine).
		Builder& operator<<(const std::string& t);
		Builder& operator<<(string_ref t);
		Builder& operator<<(const char* t);
		Builder& operator<<(unsigned char t);
		Builder& operator<<(unsigned short t);
		Builder& operator<<(unsigned t);
		Builder& operator<<(unsigned long t);
		Builder& operator<<(unsigned long long t);
		Builder& operator<<(char t);
		Builder& operator<<(short t);
		Builder& operator<<(int t);
		Builder& operator<<(long t);
		Builder& operator<<(long long t);
		Builder& operator<<(float t);
		Builder& operator<<(double t);

		// Templatized version is commented out. There's no problem in
		// enabling it, but the code works ATM without, and having it
		// disabled helps to catch future missing overloads in the
		// list above.
		/*template <typename T> Builder& operator<<(const T& t) {
			buf += toString(t); return *this;
		}*/

		//operator std::string() const;
		operator std::string() const { return buf; }
		operator string_ref()  const { return buf; }

	private:
		std::string buf;
	};

	// Generic toString implementation, works for all 'streamable' types.
	template <typename T> std::string toString(const T& t)
	{
		std::ostringstream s;
		s << t;
		return s.str();
	}
	// Overloads for specific types. These are much faster than the generic
	// version. Having them non-inline also reduces size of executable.
	std::string toString(long long a);
	std::string toString(unsigned long long a);
	std::string toString(long a);
	std::string toString(unsigned long a);
	std::string toString(int a);
	std::string toString(unsigned a);
	std::string toString(short a);
	std::string toString(unsigned short a);
	std::string toString(char a);
	std::string toString(signed char a);
	std::string toString(unsigned char a);
	std::string toString(bool a);
	// These are simple enough to inline.
	inline std::string        toString(const char* s)        { return s; }
	inline const std::string& toString(const std::string& s) { return s; }

	// Print the lower 'width' number of digits of 'a' in hex format
	// (without leading '0x' and with a-f in lower case).
	std::string toHexString(unsigned a, unsigned width);

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
