// $Id$

#include "UnicodeKeymap.hh"
#include "MSXException.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include <cstring>
#include <cstdio>

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
		parseUnicodeKeymapfile(buf, file.getSize());
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

void UnicodeKeymap::parseUnicodeKeymapfile(const byte* buf, unsigned size)
{
	unsigned i = 0;
	while (i < size) {
		string line;
		bool done = false;
		// Read one line from buffer
		// Filter out any white-space
		// Only read until # mark
		while (!done && (i < size)) {
			char c = buf[i++];
			switch (c) {
				case '#':
					while ((i < size) &&
					       (buf[i++] != '\n')) {
						// skip till end of line
					}
					done = true;
					break;
				case '\r':
				case '\t':
				case ' ':
					// skip whitespace
					break;
				case '\n':
				case '\0':
					// end of line
					done = true;
					break;
				default:
					line += c;
					break;
			}
		}

		const char* begin = line.c_str();
		const char* end = strchr(begin, ',');
		if (end != NULL) {
			// Parse first token
			// It is either a unicode value or the keyword DEADKEY
			int unicode = 0;
			bool isDeadKey =
				(end - begin) == 7 && strncmp(begin, "DEADKEY", 7) == 0;
			if (!isDeadKey) {
				bool ok;
				unicode = parseHex(begin, end, ok);
				if (!ok || unicode > 0xFFFF) {
					throw MSXException("Wrong unicode value in keymap file");
				}
			}

			// Parse second token. It must be <ROW><COL>
			begin = end + 1;
			end = strchr(begin, ',');
			if (end == NULL) {
				throw MSXException("Missing <ROW><COL> in unicode file");
			}
			bool ok;
			int rowcol = parseHex(begin, end, ok);
			if (!ok || rowcol >= 0x100) {
				throw MSXException("Wrong rowcol value in keymap file");
			}
			if ((rowcol >> 4) >= 11) {
				throw MSXException("Too high row value in keymap file");
			}
			if ((rowcol & 0x0F) >= 8) {
				throw MSXException("Too high column value in keymap file");
			}

			// Parse third token. It is an optional list of modifier keywords
			begin = end + 1;
			byte modmask = 0;
			if (strstr(begin, "SHIFT") != NULL) {
				modmask |= 1;
			}
			if (strstr(begin, "CTRL") != NULL) {
				modmask |= 2;
			}
			if (strstr(begin, "GRAPH") != NULL) {
				modmask |= 4;
			}
			if (strstr(begin, "CAPSLOCK") != NULL) {
				modmask |= 8;
			}
			if (strstr(begin, "CODE" ) != NULL) {
				modmask |= 16;
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
}

} // namespace openmsx
