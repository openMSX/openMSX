// $Id$

#ifndef STRINGOP_HH
#define STRINGOP_HH

#include "stringsp.hh"
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>
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
		// disabled helps the catch future missing overloads in the
		// list above.
		/*template <typename T> Builder& operator<<(const T& t) {
			buf << t;
			return *this;
		}*/

		operator std::string() const;

	private:
		std::ostringstream buf;
	};

	template <typename T> std::string toString(const T& t)
	{
		std::ostringstream s;
		s << t;
		return s.str();
	}

	template <typename T> std::string toHexString(const T& t, int width)
	{
		std::ostringstream s;
		s << std::hex << std::setw(width) << std::setfill('0') << t;
		return s.str();
	}
	std::string toHexString(unsigned char t, int width);

	int stringToInt(const std::string& str);
	bool stringToInt(const std::string& str, int& result);
	unsigned stringToUint(const std::string& str);
	bool stringToUint(const std::string& str, unsigned& result);
	unsigned long long stringToUint64(const std::string& str);
	bool stringToBool(const std::string& str);
	double stringToDouble(const std::string& str);
	bool stringToDouble(const std::string& str, double& result);

	std::string toLower(const std::string& str);

	bool startsWith(const std::string& total, const std::string& part);
	bool startsWith(const std::string& total, char part);
	bool endsWith  (const std::string& total, const std::string& part);
	bool endsWith  (const std::string& total, char part);

	void trimRight(std::string& str, const char* chars);
	void trimRight(std::string& str, char chars);
	void trimLeft (std::string& str, const char* chars);

	void splitOnFirst(const std::string& str, const char* chars,
	                  std::string& first, std::string& last);
	void splitOnFirst(const std::string& str, char chars,
	                  std::string& first, std::string& last);
	void splitOnLast (const std::string& str, const char* chars,
	                  std::string& first, std::string& last);
	void splitOnLast (const std::string& str, char chars,
	                  std::string& first, std::string& last);
	void split(const std::string& str, const char* chars,
	           std::vector<std::string>& result);
	std::string join(const std::vector<std::string>& elems,
	                 const std::string& separator);
	void parseRange(const std::string& str, std::set<unsigned>& result,
			unsigned min, unsigned max);

	// case insensitive less then operator
	struct caseless {
		bool operator()(const std::string& s1, const std::string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};

#if defined(__APPLE__)
	std::string fromCFString(CFStringRef str);
#endif
}

#endif
