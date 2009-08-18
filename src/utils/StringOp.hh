// $Id$

#ifndef STRINGOP_HH
#define STRINGOP_HH

#include "stringsp.hh"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

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
		Builder& operator<<(unsigned t);
		Builder& operator<<(unsigned long t);
		Builder& operator<<(int t);
		Builder& operator<<(char t);
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
	unsigned stringToUint(const std::string& str);
	unsigned long long stringToUint64(const std::string& str);
	bool stringToBool(const std::string& str);
	double stringToDouble(const std::string& str);

	std::string toLower(const std::string& str);

	bool startsWith(const std::string& total, const std::string& part);
	bool endsWith  (const std::string& total, const std::string& part);

	void trimRight(std::string& str, const std::string& chars);
	void trimLeft (std::string& str, const std::string& chars);

	void splitOnFirst(const std::string& str, const std::string& chars,
	                  std::string& first, std::string& last);
	void splitOnLast (const std::string& str, const std::string& chars,
	                  std::string& first, std::string& last);
	void split(const std::string& str, const std::string& chars,
	           std::vector<std::string>& result);

	// case insensitive less then operator
	struct caseless {
		bool operator()(const std::string& s1, const std::string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};
}

#endif
