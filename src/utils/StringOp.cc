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
	string result = strCat(*it);
	for (++it; it != end(elems); ++it) {
		strAppend(result, separator, *it);
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
	throw openmsx::MSXException("Invalid integer: ", str);
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
