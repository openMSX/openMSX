// $Id$

#include "UnicodeKeymap.hh"
#include "MSXException.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "StringOp.hh"
#include <cstdlib>
#include <cstdio>
#include <cstring>

using std::string;

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


UnicodeKeymap::UnicodeKeymap(const string& keyboardType)
	: emptyInfo(KeyInfo(0, 0, 0))
	, deadKey(KeyInfo(0, 0, 0))
{
	SystemFileContext context;
	CommandController* controller = NULL; // ok for SystemFileContext
	string filename = context.resolve(
		*controller, "unicodemaps/unicodemap." + keyboardType);
	try {
		File file(filename);
		byte* buf = file.mmap();
		parseUnicodeKeymapfile(
			reinterpret_cast<const char*>(buf),
			reinterpret_cast<const char*>(buf + file.getSize())
			);
	} catch (FileException&) {
		throw MSXException("Couldn't load unicode keymap file: " + filename);
	}
}

UnicodeKeymap::KeyInfo UnicodeKeymap::get(int unicode) const
{
	Mapdata::const_iterator it = mapdata.find(unicode);
	return (it == mapdata.end()) ? emptyInfo : it->second;
}

UnicodeKeymap::KeyInfo UnicodeKeymap::getDeadkey() const
{
	return deadKey;
}

void UnicodeKeymap::parseUnicodeKeymapfile(const char* begin, const char* end)
{
	begin = skipSep(begin, end);
	while (true) {
		// Find a line containing tokens.
		while (begin != end && *begin == '\n') {
			// Next line.
			begin = skipSep(begin + 1, end);
		}
		if (begin == end) break;

		// Parse first token: a unicode value or the keyword DEADKEY.
		const char* tokenEnd = findSep(begin, end);
		int unicode = 0;
		bool isDeadKey = segmentEquals(begin, tokenEnd, "DEADKEY");
		if (!isDeadKey) {
			bool ok;
			unicode = parseHex(begin, tokenEnd, ok);
			if (!ok || unicode > 0xFFFF) {
				throw MSXException("Wrong unicode value in keymap file");
			}
		}
		begin = skipSep(tokenEnd, end);

		// Parse second token. It must be <ROW><COL>
		tokenEnd = findSep(begin, end);
		if (tokenEnd == end) {
			throw MSXException("Missing <ROW><COL> in unicode file");
		}
		bool ok;
		int rowcol = parseHex(begin, tokenEnd, ok);
		if (!ok || rowcol >= 0x100) {
			throw MSXException("Wrong rowcol value in keymap file");
		}
		if ((rowcol >> 4) >= 11) {
			throw MSXException("Too high row value in keymap file");
		}
		if ((rowcol & 0x0F) >= 8) {
			throw MSXException("Too high column value in keymap file");
		}
		begin = skipSep(tokenEnd, end);

		// Parse remaining tokens. It is an optional list of modifier keywords.
		byte modmask = 0;
		while (begin != end && *begin != '\n') {
			tokenEnd = findSep(begin, end);
			if (segmentEquals(begin, tokenEnd, "SHIFT")) {
				modmask |= 1;
			} else if (segmentEquals(begin, tokenEnd, "CTRL")) {
				modmask |= 2;
			} else if (segmentEquals(begin, tokenEnd, "GRAPH")) {
				modmask |= 4;
			} else if (segmentEquals(begin, tokenEnd, "CAPSLOCK")) {
				modmask |= 8;
			} else if (segmentEquals(begin, tokenEnd, "CODE")) {
				modmask |= 16;
			} else {
				char *s = strndup(begin, tokenEnd - begin);
				string msg = StringOp::Builder()
					<< "Invalid modifier \"" << s << "\" in keymap file";
				free(s);
				throw MSXException(msg);
			}
			begin = skipSep(tokenEnd, end);
		}

		if (isDeadKey) {
			deadKey.row = (rowcol >> 4) & 0x0f;
			deadKey.keymask = 1 << (rowcol & 7);
			deadKey.modmask = 0;
		} else {
			KeyInfo info((rowcol >> 4) & 0x0f, // row
							1 << (rowcol & 7),    // keymask
							modmask);             // modmask
			mapdata.insert(std::make_pair(unicode, info));
		}
	}
}

} // namespace openmsx
