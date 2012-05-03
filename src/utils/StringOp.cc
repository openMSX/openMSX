#include "StringOp.hh"
#include "MSXException.hh"
#include <algorithm>
#include <limits>
#include <cassert>
#include <cstdlib>
#include <stdexcept>

using std::string;
using std::transform;
using std::vector;
using std::set;

namespace StringOp {

// class Builder

Builder::Builder() = default;

Builder::~Builder() = default;

Builder& Builder::operator<<(const std::string& t)
{
	buf += t; return *this;
}
Builder& Builder::operator<<(string_ref t)
{
	buf.append(t.data(), t.size()); return *this;
}
Builder& Builder::operator<<(const char* t)
{
	buf += t; return *this;
}
Builder& Builder::operator<<(unsigned char t)
{
	return operator<<(unsigned(t));
}
Builder& Builder::operator<<(unsigned short t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(unsigned t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(unsigned long t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(unsigned long long t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(char t)
{
	buf += t; return *this;
}
Builder& Builder::operator<<(short t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(int t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(long t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(long long t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(float t)
{
	buf += toString(t); return *this;
}
Builder& Builder::operator<<(double t)
{
	buf += toString(t); return *this;
}


// Returns a fast type that is (at least) big enough to hold the absolute value
// of values of the given type. (It always returns 'unsigned' except for 64-bit
// integers it returns unsigned long long).
template<typename T> struct FastUnsigned           { using type = unsigned; };
template<> struct FastUnsigned<long long>          { using type = unsigned long long; };
template<> struct FastUnsigned<unsigned long long> { using type = unsigned long long; };
template<> struct FastUnsigned<long>               { using type = unsigned long; };
template<> struct FastUnsigned<unsigned long>      { using type = unsigned long; };

// This does the equivalent of
//   unsigned u = (t < 0) ? -t : t;
// but it avoids a compiler warning on the operations
//   't < 0' and '-t'
// when 't' is actually an unsigned type.
template<bool IS_SIGNED> struct AbsHelper;
template<> struct AbsHelper<true> {
	template<typename T>
	inline typename FastUnsigned<T>::type operator()(T t) const {
		return (t < 0) ? -t : t;
	}
};
template<> struct AbsHelper<false> {
	template<typename T>
	inline typename FastUnsigned<T>::type operator()(T t) const {
		return t;
	}
};

// Does the equivalent of   if (t < 0) *--p = '-';
// but it avoids a compiler warning on 't < 0' when 't' is an unsigned type.
template<bool IS_SIGNED> struct PutSign;
template<> struct PutSign<true> {
	template<typename T> inline void operator()(T t, char*& p) const {
		if (t < 0) *--p = '-';
	}
};
template<> struct PutSign<false> {
	template<typename T> inline void operator()(T /*t*/, char*& /*p*/) const {
		// nothing
	}
};

// This routine is inspired by boost::lexical_cast. It's much faster than a
// generic version using std::stringstream. See this page for some numbers:
// http://www.boost.org/doc/libs/1_47_0/libs/conversion/lexical_cast.htm#performance
template<typename T> static inline string toStringImpl(T t)
{
	static const bool IS_SIGNED = std::numeric_limits<T>::is_signed;
	static const unsigned BUF_SIZE = 1 + std::numeric_limits<T>::digits10
	                               + (IS_SIGNED ? 1 : 0);

	char buf[BUF_SIZE];
	char* p = &buf[BUF_SIZE];

	AbsHelper<IS_SIGNED> absHelper;
	typename FastUnsigned<T>::type a = absHelper(t);
	do {
		*--p = '0' + (a % 10);
		a /= 10;
	} while (a);

	PutSign<IS_SIGNED> putSign;
	putSign(t, p);

	return string(p, &buf[BUF_SIZE] - p);
}
string toString(long long a)          { return toStringImpl(a); }
string toString(unsigned long long a) { return toStringImpl(a); }
string toString(long a)               { return toStringImpl(a); }
string toString(unsigned long a)      { return toStringImpl(a); }
string toString(int a)                { return toStringImpl(a); }
string toString(unsigned a)           { return toStringImpl(a); }
string toString(short a)              { return toStringImpl(a); }
string toString(unsigned short a)     { return toStringImpl(a); }
string toString(char a)               { return string(1, a); }
string toString(signed char a)        { return string(1, a); }
string toString(unsigned char a)      { return string(1, a); }
string toString(bool a)               { return string(1, '0' + a); }

static inline char hexDigit(unsigned x)
{
	return (x < 10) ? ('0' + x) : ('a' + x - 10);
}
string toHexString(unsigned x, unsigned width)
{
	assert((0 < width) && (width <= 8));

	char buf[8];
	char* p = &buf[8];
	int i = width;
	do {
		*--p = hexDigit(x & 15);
		x >>= 4;
	} while (--i);
	return string(p, width);
}

int stringToInt(const string& str)
{
	return strtol(str.c_str(), nullptr, 0);
}
bool stringToInt(const string& str, int& result)
{
	char* endptr;
	result = strtol(str.c_str(), &endptr, 0);
	return *endptr == '\0';
}

unsigned stringToUint(const string& str)
{
	return strtoul(str.c_str(), nullptr, 0);
}
bool stringToUint(const string& str, unsigned& result)
{
	char* endptr;
	result = strtoul(str.c_str(), &endptr, 0);
	return *endptr == '\0';
}

uint64_t stringToUint64(const string& str)
{
       return strtoull(str.c_str(), nullptr, 0);
}

bool stringToBool(string_ref str)
{
	if (str == "1") return true;
	if ((str.size() == 4) && (strncasecmp(str.data(), "true", 4) == 0))
		return true;
	if ((str.size() == 3) && (strncasecmp(str.data(), "yes", 3) == 0))
		return true;
	return false;
}

double stringToDouble(const string& str)
{
	return strtod(str.c_str(), nullptr);
}
bool stringToDouble(const string& str, double& result)
{
	char* endptr;
	result = strtod(str.c_str(), &endptr);
	return *endptr == '\0';
}

string toLower(string_ref str)
{
	string result = str.str();
	transform(begin(result), end(result), begin(result), ::tolower);
	return result;
}

bool startsWith(string_ref total, string_ref part)
{
	return total.starts_with(part);
}
bool startsWith(string_ref total, char part)
{
	return !total.empty() && (total.front() == part);
}

bool endsWith(string_ref total, string_ref part)
{
	return total.ends_with(part);
}
bool endsWith(string_ref total, char part)
{
	return !total.empty() && (total.back() == part);
}

void trimRight(string& str, const char* chars)
{
	auto pos = str.find_last_not_of(chars);
	if (pos != string::npos) {
		str.erase(pos + 1);
	} else {
		str.clear();
	}
}
void trimRight(string& str, char chars)
{
	auto pos = str.find_last_not_of(chars);
	if (pos != string::npos) {
		str.erase(pos + 1);
	} else {
		str.clear();
	}
}
void trimRight(string_ref& str, string_ref chars)
{
	while (!str.empty() && (chars.find(str.back()) != string_ref::npos)) {
		str.pop_back();
	}
}
void trimRight(string_ref& str, char chars)
{
	while (!str.empty() && (str.back() == chars)) {
		str.pop_back();
	}
}

void trimLeft(string& str, const char* chars)
{
	str.erase(0, str.find_first_not_of(chars));
}
void trimLeft(string& str, char chars)
{
	str.erase(0, str.find_first_not_of(chars));
}
void trimLeft(string_ref& str, string_ref chars)
{
	while (!str.empty() && (chars.find(str.front()) != string_ref::npos)) {
		str.pop_front();
	}
}
void trimLeft(string_ref& str, char chars)
{
	while (!str.empty() && (str.front() == chars)) {
		str.pop_front();
	}
}

void trim(string_ref& str, string_ref chars)
{
	trimRight(str, chars);
	trimLeft (str, chars);
}

void trim(string_ref& str, char chars)
{
	trimRight(str, chars);
	trimLeft (str, chars);
}

void splitOnFirst(string_ref str, string_ref chars, string_ref& first, string_ref& last)
{
	auto pos = str.find_first_of(chars);
	if (pos == string_ref::npos) {
		first = str;
		last.clear();
	} else {
		first = str.substr(0, pos);
		last  = str.substr(pos + 1);
	}
}
void splitOnFirst(string_ref str, char chars, string_ref& first, string_ref& last)
{
	auto pos = str.find_first_of(chars);
	if (pos == string_ref::npos) {
		first = str;
		last.clear();
	} else {
		first = str.substr(0, pos);
		last  = str.substr(pos + 1);
	}
}

void splitOnLast(string_ref str, string_ref chars, string_ref& first, string_ref& last)
{
	auto pos = str.find_last_of(chars);
	if (pos == string_ref::npos) {
		first.clear();
		last = str;
	} else {
		first = str.substr(0, pos);
		last  = str.substr(pos + 1);
	}
}
void splitOnLast(string_ref str, char chars, string_ref& first, string_ref& last)
{
	auto pos = str.find_last_of(chars);
	if (pos == string_ref::npos) {
		first.clear();
		last = str;
	} else {
		first = str.substr(0, pos);
		last  = str.substr(pos + 1);
	}
}

vector<string_ref> split(string_ref str, char chars)
{
	vector<string_ref> result;
	while (!str.empty()) {
		string_ref first, last;
		splitOnFirst(str, chars, first, last);
		result.push_back(first);
		str = last;
	}
	return result;
}

string join(const vector<string_ref>& elems, char separator)
{
	if (elems.empty()) return {};

	auto it = begin(elems);
	Builder result;
	result << *it;
	for (++it; it != end(elems); ++it) {
		result << separator;
		result << *it;
	}
	return result;
}

static unsigned parseNumber(string_ref str)
{
	trim(str, " \t");
	if (!str.empty()) {
		try {
			return fast_stou(str);
		} catch (std::invalid_argument&) {
			// parse error
		}
	}
	throw openmsx::MSXException("Invalid integer: " + str);
}

static void insert(unsigned x, set<unsigned>& result, unsigned min, unsigned max)
{
	if ((x < min) || (x > max)) {
		throw openmsx::MSXException("Out of range");
	}
	result.insert(x);
}

static void parseRange2(string_ref str, set<unsigned>& result,
                        unsigned min, unsigned max)
{
	// trimRight only: here we only care about all spaces
	trimRight(str, " \t");
	if (str.empty()) return;

	auto pos = str.find('-');
	if (pos == string_ref::npos) {
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

set<unsigned> parseRange(string_ref str, unsigned min, unsigned max)
{
	set<unsigned> result;
	while (true) {
		auto next = str.find(',');
		string_ref sub = (next == string_ref::npos)
		               ? str
		               : str.substr(0, next++);
		parseRange2(sub, result, min, max);
		if (next == string_ref::npos) break;
		str = str.substr(next);
	}
	return result;
}

#if defined(__APPLE__)

std::string fromCFString(CFStringRef str)
{
	// Try the quick route first.
	const char *cstr = CFStringGetCStringPtr(str, kCFStringEncodingUTF8);
	if (cstr) {
		// String was already in UTF8 encoding.
		return std::string(cstr);
	}

	// Convert to UTF8 encoding.
	CFIndex len = CFStringGetLength(str);
	CFRange range = CFRangeMake(0, len);
	CFIndex usedBufLen = 0;
	CFStringGetBytes(
		str, range, kCFStringEncodingUTF8, '?', false, nullptr, len, &usedBufLen);
	UInt8 buffer[usedBufLen];
	CFStringGetBytes(
		str, range, kCFStringEncodingUTF8, '?', false, buffer, len, &usedBufLen);
	return std::string(reinterpret_cast<const char *>(buffer), usedBufLen);
}

#endif

} // namespace StringOp
