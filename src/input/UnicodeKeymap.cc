// $Id: Keyboard.cc 7559 2008-01-17 23:37:36Z awulms $

#include <string.h>
#include "MSXException.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "UnicodeKeymap.hh"

using std::string;
//using std::vector;

namespace openmsx {

UnicodeKeymap::KeyInfo UnicodeKeymap::get(int unicode)
{
	std::map<int, KeyInfo>::const_iterator it = mapdata.find(unicode);
	if (it == mapdata.end())
	{
		return emptyInfo;
	}
	else
	{
		return it->second;
	}
}

UnicodeKeymap::KeyInfo UnicodeKeymap::getDeadkey()
{
	return deadKey;
}

void UnicodeKeymap::parseUnicodeKeymapfile(const byte* buf, unsigned size)
{
	unsigned i = 0;
	while  (i < size) {
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

		char *token = strtok(charbuf, ",");
		if (token != NULL)
		{
			// Parse first token
			// It is either a unicode value or the keyword DEADKEY
			int unicode;
			bool isDeadKey = (strcmp(token, "DEADKEY") == 0);
			if (!isDeadKey) {
				sscanf(token, "%x", &unicode);
				if (unicode < 0 || unicode > 65535) {
					throw MSXException("Wrong unicode value in keymap file");
				}
			}

			// Parse second token. It must be <ROW><COL>
			token = strtok(NULL, ",");
			if (token == NULL) {
				throw MSXException("Missing <ROW><COL> in unicode file");
			}
			int rowcol;
			sscanf(token, "%x", &rowcol);
			if (rowcol < 0 || rowcol >= 11*16 ) {
				throw MSXException("Wrong rowcol value in keymap file");
			}
			if ((rowcol & 0x0f) > 7) {
				throw MSXException("Too high column value in keymap file");
			}

			// Parse third token. It is an optional list of modifier keywords
			token = strtok(NULL, ",");
			byte modmask = 0;
			if (token != NULL) {
				if (strstr(token, "SHIFT") != NULL) {
					modmask |= 1;
				}
				if (strstr(token, "CTRL") != NULL) {
					modmask |= 2;
				}
				if (strstr(token, "GRAPH") != NULL) {
					modmask |= 4;
				}
				if (strstr(token, "CAPSLOCK") != NULL) {
					modmask |= 8;
				}
				if (strstr(token, "CODE" ) != NULL) {
					modmask |= 16;
				}
			}

			if (isDeadKey)
			{
				deadKey.row = (rowcol >> 4) & 0x0f;
				deadKey.keymask = 1 << (rowcol & 7);
				deadKey.modmask = 0;
			}
			else
			{
				mapdata[unicode].row = (rowcol >> 4) & 0x0f;
				mapdata[unicode].keymask = 1 << (rowcol & 7);
				mapdata[unicode].modmask = modmask;
			}
		}
	}
}

UnicodeKeymap::UnicodeKeymap(string& keyboardType)
	: emptyInfo(KeyInfo(0,0,0))
	, deadKey(KeyInfo(0,0,0))
{
	SystemFileContext context;
	string filename = context.resolve("unicodemaps/unicodemap." + keyboardType);
	try {
		File file(filename);
		byte* buf = file.mmap();
		parseUnicodeKeymapfile(buf, file.getSize());
	} catch (FileException &e) {
		throw MSXException("Couldn't load unicode keymap file: " + filename);
	}
}

} // namespace openmsx

