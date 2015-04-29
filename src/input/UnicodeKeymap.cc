#include "UnicodeKeymap.hh"
#include "MSXException.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "StringOp.hh"
#include "stl.hh"
#include <algorithm>
#include <cstring>

namespace openmsx {

/** Parses the string given by the inclusive begin point and exclusive end
  * pointer as a hexadecimal integer.
  * If successful, returns the parsed value and sets "ok" to true.
  * If unsuccessful, returns 0 and sets "ok" to false.
  */
static unsigned parseHex(const char* begin, const char* end, bool& ok)
{
	unsigned value = 0;
	for (; begin != end; begin++) {
		value *= 16;
		const char c = *begin;
		if ('0' <= c && c <= '9') {
			value += c - '0';
		} else if ('A' <= c && c <= 'F') {
			value += c - 'A' + 10;
		} else if ('a' <= c && c <= 'f') {
			value += c - 'a' + 10;
		} else {
			ok = false;
			return 0;
		}
	}
	ok = true;
	return value;
}

/** Returns true iff the given character is a separator.
  * Separators are: comma, whitespace and hash mark.
  */
static inline bool isSep(char c)
{
	return c == ','                           // comma
	    || c == ' ' || c == '\t' || c == '\r' // whitespace
	    || c == '#';                          // comment
}

/** Returns a pointer to the first separator character in the given string,
  * or a pointer to the end of the line if no separator is found.
  */
static const char* findSep(const char* begin, const char* end)
{
	while (begin != end && *begin != '\n' && !isSep(*begin)) begin++;
	return begin;
}

/** Returns a pointer to the first non-separator character in the given string,
  * or a pointer to the end of the line if only separators are found.
  * Characters between a hash mark and the following newline are also skipped.
  */
static const char* skipSep(const char* begin, const char* end)
{
	while (begin != end) {
		const char c = *begin;
		if (!isSep(c)) break;
		begin++;
		if (c == '#') {
			// Skip till end of line.
			while (begin != end && *begin != '\n') begin++;
			break;
		}
	}
	return begin;
}

/** Return true iff the substring [begin, end) equals the given string literal
 */
template<int N>
static bool segmentEquals(const char* begin, const char* end, const char (&string)[N])
{
	return ((end - begin) == (N - 1)) &&
	       (strncmp(begin, string, N - 1) == 0);
}

/** Return true iff the substring [begin, end) starts with the given string literal
 */
template<int N>
static bool segmentStartsWith(const char* begin, const char* end, const char (&string)[N])
{
	return ((end - begin) >= (N - 1)) &&
	       (strncmp(begin, string, N - 1) == 0);
}


UnicodeKeymap::UnicodeKeymap(string_ref keyboardType)
	: emptyInfo(KeyInfo())
{
	auto filename = systemFileContext().resolve(
		"unicodemaps/unicodemap." + keyboardType);
	try {
		File file(filename);
		size_t size;
		const byte* buf = file.mmap(size);
		parseUnicodeKeymapfile(
			reinterpret_cast<const char*>(buf),
			reinterpret_cast<const char*>(buf + size));
	} catch (FileException&) {
		throw MSXException("Couldn't load unicode keymap file: " + filename);
	}
}

UnicodeKeymap::KeyInfo UnicodeKeymap::get(int unicode) const
{
	auto it = lower_bound(begin(mapdata), end(mapdata), unicode,
	                      LessTupleElement<0>());
	return ((it != end(mapdata)) && (it->first == unicode))
		? it->second : emptyInfo;
}

UnicodeKeymap::KeyInfo UnicodeKeymap::getDeadkey(unsigned n) const
{
	assert(n < NUM_DEAD_KEYS);
	return deadKeys[n];
}

void UnicodeKeymap::parseUnicodeKeymapfile(const char* b, const char* e)
{
	b = skipSep(b, e);
	while (true) {
		// Find a line containing tokens.
		while (b != e && *b == '\n') {
			// Next line.
			b = skipSep(b + 1, e);
		}
		if (b == e) break;

		// Parse first token: a unicode value or the keyword DEADKEY.
		const char* tokenEnd = findSep(b, e);
		int unicode = 0;
		unsigned deadKeyIndex = 0;
		bool isDeadKey = segmentStartsWith(b, tokenEnd, "DEADKEY");
		if (isDeadKey) {
			const char* begin2 = b + strlen("DEADKEY");
			if (begin2 == tokenEnd) {
				// The normal keywords are
				//    DEADKEY1  DEADKEY2  DEADKEY3
				// but for backwards compatibility also still recognize
				//    DEADKEY
			} else {
				bool ok;
				deadKeyIndex = parseHex(begin2, tokenEnd, ok);
				deadKeyIndex--; // Make index 0 based instead of 1 based
				if (!ok || deadKeyIndex >= NUM_DEAD_KEYS) {
					throw MSXException(StringOp::Builder() <<
						"Wrong deadkey number in keymap file. "
						"It must be 1.." << NUM_DEAD_KEYS);
				}
			}
		} else {
			bool ok;
			unicode = parseHex(b, tokenEnd, ok);
			if (!ok || unicode > 0xFFFF) {
				throw MSXException("Wrong unicode value in keymap file");
			}
		}
		b = skipSep(tokenEnd, e);

		// Parse second token. It must be <ROW><COL>
		tokenEnd = findSep(b, e);
		if (tokenEnd == e) {
			throw MSXException("Missing <ROW><COL> in unicode file");
		}
		bool ok;
		int rowcol = parseHex(b, tokenEnd, ok);
		if (!ok || rowcol >= 0x100) {
			throw MSXException("Wrong rowcol value in keymap file");
		}
		if ((rowcol >> 4) >= 11) {
			throw MSXException("Too high row value in keymap file");
		}
		if ((rowcol & 0x0F) >= 8) {
			throw MSXException("Too high column value in keymap file");
		}
		b = skipSep(tokenEnd, e);

		// Parse remaining tokens. It is an optional list of modifier keywords.
		byte modmask = 0;
		while (b != e && *b != '\n') {
			tokenEnd = findSep(b, e);
			if (segmentEquals(b, tokenEnd, "SHIFT")) {
				modmask |= 1;
			} else if (segmentEquals(b, tokenEnd, "CTRL")) {
				modmask |= 2;
			} else if (segmentEquals(b, tokenEnd, "GRAPH")) {
				modmask |= 4;
			} else if (segmentEquals(b, tokenEnd, "CAPSLOCK")) {
				modmask |= 8;
			} else if (segmentEquals(b, tokenEnd, "CODE")) {
				modmask |= 16;
			} else {
				throw MSXException(StringOp::Builder()
					<< "Invalid modifier \""
					<< string_ref(b, tokenEnd)
					<< "\" in keymap file");
			}
			b = skipSep(tokenEnd, e);
		}

		if (isDeadKey) {
			deadKeys[deadKeyIndex] = KeyInfo(
				(rowcol >> 4) & 0x0f, // row
				1 << (rowcol & 7),    // keymask
				0);                   // modmask
		} else {
			mapdata.emplace_back(unicode, KeyInfo(
				(rowcol >> 4) & 0x0f, // row
				1 << (rowcol & 7),    // keymask
				modmask));            // modmask
		}
	}
	sort(begin(mapdata), end(mapdata), LessTupleElement<0>());
}

} // namespace openmsx
