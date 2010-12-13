// $Id$

#include "cstdlibp.hh"
#include <algorithm>
#include "StringOp.hh"
#include "MSXException.hh"

using std::advance;
using std::equal;
using std::string;
using std::transform;
using std::vector;
using std::set;

namespace StringOp {

// class Builder

Builder::Builder()
{
}

Builder::~Builder()
{
}

Builder& Builder::operator<<(const std::string& t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(const char* t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(unsigned char t)
{
	return operator<<(unsigned(t));
}
Builder& Builder::operator<<(unsigned short t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(unsigned t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(unsigned long t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(unsigned long long t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(char t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(short t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(int t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(long t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(long long t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(float t)
{
	buf << t; return *this;
}
Builder& Builder::operator<<(double t)
{
	buf << t; return *this;
}

Builder::operator std::string() const
{
	return buf.str();
}



std::string toHexString(unsigned char t, int width)
{
	// promote byte to int before printing
	return toHexString(unsigned(t), width);
}

int stringToInt(const string& str)
{
	return strtol(str.c_str(), NULL, 0);
}
bool stringToInt(const string& str, int& result)
{
	char* endptr;
	result = strtol(str.c_str(), &endptr, 0);
	return *endptr == '\0';
}

unsigned stringToUint(const string& str)
{
	return strtoul(str.c_str(), NULL, 0);
}
bool stringToUint(const string& str, unsigned& result)
{
	char* endptr;
	result = strtoul(str.c_str(), &endptr, 0);
	return *endptr == '\0';
}

unsigned long long stringToUint64(const string& str)
{
       return strtoull(str.c_str(), NULL, 0);
}

bool stringToBool(const string& str)
{
	string low = toLower(str);
	return (low == "true") || (low == "yes") || (low == "1");
}

double stringToDouble(const string& str)
{
	return strtod(str.c_str(), NULL);
}
bool stringToDouble(const string& str, double& result)
{
	char* endptr;
	result = strtod(str.c_str(), &endptr);
	return *endptr == '\0';
}

string toLower(const string& str)
{
	string result = str;
	transform(result.begin(), result.end(), result.begin(), ::tolower);
	return result;
}

bool startsWith(const string& total, const string& part)
{
	if (total.size() < part.size()) return false;
	return equal(part.begin(), part.end(), total.begin());
}
bool startsWith(const string& total, char part)
{
	return !total.empty() && (total[0] == part);
}

bool endsWith(const string& total, const string& part)
{
	string::difference_type offset = total.size() - part.size();
	if (offset < 0) return false;
	return equal(part.begin(), part.end(), total.begin() + offset);
}
bool endsWith(const string& total, char part)
{
	return !total.empty() && (*total.rbegin() == part);
}

void trimRight(string& str, const char* chars)
{
	string::size_type pos = str.find_last_not_of(chars);
	if (pos != string::npos) {
		str.erase(pos + 1);
	} else {
		str.clear();
	}
}
void trimRight(string& str, char chars)
{
	string::size_type pos = str.find_last_not_of(chars);
	if (pos != string::npos) {
		str.erase(pos + 1);
	} else {
		str.clear();
	}
}
void trimLeft (string& str, const char* chars)
{
	str.erase(0, str.find_first_not_of(chars));
}

void splitOnFirst(const string& str, const char* chars, string& first, string& last)
{
	std::string::size_type pos = str.find_first_of(chars);
	if (pos == std::string::npos) {
		first = str;
		last.clear();
	} else {
		first = str.substr(0, pos);
		last  = str.substr(pos + 1);
	}
}
void splitOnFirst(const string& str, char chars, string& first, string& last)
{
	std::string::size_type pos = str.find_first_of(chars);
	if (pos == std::string::npos) {
		first = str;
		last.clear();
	} else {
		first = str.substr(0, pos);
		last  = str.substr(pos + 1);
	}
}

void splitOnLast(const string& str, const char* chars, string& first, string& last)
{
	std::string::size_type pos = str.find_last_of(chars);
	if (pos == std::string::npos) {
		first.clear();
		last = str;
	} else {
		first = str.substr(0, pos);
		last  = str.substr(pos + 1);
	}
}
void splitOnLast(const string& str, char chars, string& first, string& last)
{
	std::string::size_type pos = str.find_last_of(chars);
	if (pos == std::string::npos) {
		first.clear();
		last = str;
	} else {
		first = str.substr(0, pos);
		last  = str.substr(pos + 1);
	}
}

void split(const string& str, const char* chars, vector<string>& result)
{
	// can be implemented more efficiently if needed
	string tmp = str;
	while (!tmp.empty()) {
		string first, last;
		splitOnFirst(tmp, chars, first, last);
		result.push_back(first);
		tmp = last;
	}
}

string join(const vector<string>& elems, const string& separator)
{
	if (elems.empty()) return string();

	vector<string>::const_iterator it = elems.begin();
	string result = *it;
	for (++it; it != elems.end(); ++it) {
		result += separator;
		result += *it;
	}
	return result;
}

static unsigned parseNumber(string str)
{
	// trimRight only: strtoul can handle leading spaces
	trimRight(str, " \t");
	if (str.empty()) {
		throw openmsx::MSXException("Invalid integer: empty string");
	}
	unsigned result;
	if (!stringToUint(str, result)) {
		throw openmsx::MSXException("Invalid integer: " + str);
	}
	return result;
}

static void insert(unsigned x, set<unsigned>& result, unsigned min, unsigned max)
{
	if ((x < min) || (x > max)) {
		throw openmsx::MSXException("Out of range");
	}
	result.insert(x);
}

static void parseRange2(string str, set<unsigned>& result,
                        unsigned min, unsigned max)
{
	// trimRight only: here we only care about all spaces
	trimRight(str, " \t");
	if (str.empty()) return;

	string::size_type pos = str.find('-');
	if (pos == string::npos) {
		insert(parseNumber(str), result, min, max);
	} else {
		unsigned begin = parseNumber(str.substr(0, pos));
		unsigned end   = parseNumber(str.substr(pos + 1));
		if (end < begin) {
			std::swap(begin, end);
		}
		for (unsigned i = begin; i <= end; ++i) {
			insert(i, result, min, max);
		}
	}
}

void parseRange(const string& str, set<unsigned>& result,
		unsigned min, unsigned max)
{
	string::size_type prev = 0;
	while (prev != string::npos) {
		string::size_type next = str.find(',', prev);
		string sub = (next == string::npos)
		           ? str.substr(prev)
		           : str.substr(prev, next++ - prev);
		parseRange2(sub, result, min, max);
		prev = next;
	}
}

} // namespace StringOp
