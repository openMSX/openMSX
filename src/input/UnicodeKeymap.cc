// $Id: Keyboard.cc 7559 2008-01-17 23:37:36Z awulms $

#include "UnicodeKeymap.hh"
#include "MSXException.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include <cstring>
#include <cstdio>

using std::string;

namespace openmsx {

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
		char charbuf[100];
		strncpy(charbuf, line.c_str(), sizeof(charbuf));
		charbuf[sizeof(charbuf)-1]=0;

		const char* begin = charbuf;
		const char* end = strchr(begin, ',');
		if (end != NULL) {
			char token[sizeof(charbuf)];
			strncpy(token, begin, end - begin);
			token[end - begin] = '\0';
			// Parse first token
			// It is either a unicode value or the keyword DEADKEY
			int unicode = 0;
			bool isDeadKey = (strcmp(token, "DEADKEY") == 0);
			if (!isDeadKey) {
				int rdnum = sscanf(token, "%x", &unicode);
				if (rdnum != 1 || unicode < 0 || unicode > 65535) {
					throw MSXException("Wrong unicode value in keymap file");
				}
			}

			// Parse second token. It must be <ROW><COL>
			begin = end + 1;
			end = strchr(begin, ',');
			if (end == NULL) {
				throw MSXException("Missing <ROW><COL> in unicode file");
			}
			strncpy(token, begin, end - begin);
			token[end - begin] = '\0';
			int rowcol;
			int rdnum = sscanf(token, "%x", &rowcol);
			if (rdnum != 1 || rowcol < 0 || rowcol >= 11*16 ) {
				throw MSXException("Wrong rowcol value in keymap file");
			}
			if ((rowcol & 0x0f) > 7) {
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
