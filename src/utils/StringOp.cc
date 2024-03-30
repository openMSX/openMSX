#include "StringOp.hh"

#include "MSXException.hh"

#include "ranges.hh"
#include "stl.hh"

#include <bit>
#include <cstdlib>

using std::string;
using std::string_view;

namespace StringOp {

bool stringToBool(string_view str)
{
	if (str == "1") return true;
	if ((str.size() == 4) && (strncasecmp(str.data(), "true", 4) == 0))
		return true;
	if ((str.size() == 3) && (strncasecmp(str.data(), "yes", 3) == 0))
		return true;
	return false;
}

std::string toLower(std::string_view str)
{
	std::string result(str);
	transform_in_place(result, ::tolower);
	return result;
}

void trimRight(string& str, const char* chars)
{
	if (auto pos = str.find_last_not_of(chars); pos != string::npos) {
		str.erase(pos + 1);
	} else {
		str.clear();
	}
}
void trimRight(string& str, char chars)
{
	if (auto pos = str.find_last_not_of(chars); pos != string::npos) {
		str.erase(pos + 1);
	} else {
		str.clear();
	}
}
void trimRight(string_view& str, string_view chars)
{
	while (!str.empty() && (chars.find(str.back()) != string_view::npos)) {
		str.remove_suffix(1);
	}
}
void trimRight(string_view& str, char chars)
{
	while (!str.empty() && (str.back() == chars)) {
		str.remove_suffix(1);
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
void trimLeft(string_view& str, string_view chars)
{
	while (!str.empty() && (chars.find(str.front()) != string_view::npos)) {
		str.remove_prefix(1);
	}
}
void trimLeft(string_view& str, char chars)
{
	while (!str.empty() && (str.front() == chars)) {
		str.remove_prefix(1);
	}
}

void trim(string_view& str, string_view chars)
{
	trimRight(str, chars);
	trimLeft (str, chars);
}

void trim(string_view& str, char chars)
{
	trimRight(str, chars);
	trimLeft (str, chars);
}

std::pair<string_view, string_view> splitOnFirst(string_view str, string_view chars)
{
	if (auto pos = str.find_first_of(chars); pos == string_view::npos) {
		return {str, string_view{}};
	} else {
		return {str.substr(0, pos), str.substr(pos + 1)};
	}
}
std::pair<string_view, string_view> splitOnFirst(string_view str, char chars)
{
	if (auto pos = str.find_first_of(chars); pos == string_view::npos) {
		return {str, string_view{}};
	} else {
		return {str.substr(0, pos), str.substr(pos + 1)};
	}
}

std::pair<string_view, string_view> splitOnLast(string_view str, string_view chars)
{
	if (auto pos = str.find_last_of(chars); pos == string_view::npos) {
		return {string_view{}, str};
	} else {
		return {str.substr(0, pos), str.substr(pos + 1)};
	}
}
std::pair<string_view, string_view> splitOnLast(string_view str, char chars)
{
	if (auto pos = str.find_last_of(chars); pos == string_view::npos) {
		return {string_view{}, str};
	} else {
		return {str.substr(0, pos), str.substr(pos + 1)};
	}
}

//std::vector<string_view> split(string_view str, char chars)
//{
//	std::vector<string_view> result;
//	while (!str.empty()) {
//		auto [first, last] = splitOnFirst(str, chars);
//		result.push_back(first);
//		str = last;
//	}
//	return result;
//}

static unsigned parseNumber(string_view str)
{
	trim(str, " \t");
	auto r = stringToBase<10, unsigned>(str);
	if (!r) {
		throw openmsx::MSXException("Invalid integer: ", str);
	}
	return *r;
}

static void checkRange(unsigned x, unsigned min, unsigned max)
{
	if ((min <= x) && (x <= max)) return;
	throw openmsx::MSXException(
		"Out of range, should be between [, ", min, ", ", max,
		"], but got: ", x);
}

static void parseRange2(string_view str, IterableBitSet<64>& result,
                        unsigned min, unsigned max)
{
	// trimRight only: here we only care about all spaces
	trimRight(str, " \t");
	if (str.empty()) return;

	if (auto pos = str.find('-'); pos == string_view::npos) {
		auto x = parseNumber(str);
		checkRange(x, min, max);
		result.set(x);
	} else {
		unsigned begin = parseNumber(str.substr(0, pos));
		unsigned end   = parseNumber(str.substr(pos + 1));
		if (end < begin) {
			std::swap(begin, end);
		}
		checkRange(begin, min, max);
		checkRange(end,   min, max);
		result.setRange(begin, end + 1);
	}
}

IterableBitSet<64> parseRange(string_view str, unsigned min, unsigned max)
{
	assert(min <= max);
	assert(max < 64);

	IterableBitSet<64> result;
	while (true) {
		auto next = str.find(',');
		string_view sub = (next == string_view::npos)
		               ? str
		               : str.substr(0, next++);
		parseRange2(sub, result, min, max);
		if (next == string_view::npos) break;
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
	return std::string(reinterpret_cast<const char*>(buffer), usedBufLen);
}

#endif

} // namespace StringOp
