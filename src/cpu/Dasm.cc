// $Id$

#include "Dasm.hh"
#include "DasmTables.hh"
#include <cstdio>

namespace openmsx {

static char sign(unsigned char a)
{
	return (a & 128) ? '-' : '+';
}

static int abs(unsigned char a)
{
	return (a & 128) ? (256 - a) : a;
}

int dasm(const byte* buffer, unsigned pc, std::string& dest)
{
	const char* s;
	char buf[10];
	int i = 0;
	char offset = 0;
	const char* r = 0;

	switch (buffer[i]) {
		case 0xCB:
			i++;
			s = mnemonic_cb[buffer[i++]];
			break;
		case 0xED:
			i++;
			s = mnemonic_ed[buffer[i++]];
			break;
		case 0xDD:
			i++;
			r = "ix";
			switch (buffer[i]) {
				case 0xcb:
					i++;
					offset = buffer[i++];
					s = mnemonic_xx_cb[buffer[i++]];
					break;
				default:
					s = mnemonic_xx[buffer[i++]];
					break;
			}
			break;
		case 0xFD:
			i++;
			r = "iy";
			switch (buffer[i]) {
				case 0xcb:
					i++;
					offset = buffer[i++];
					s = mnemonic_xx_cb[buffer[i++]];
					break;
				default:
					s = mnemonic_xx[buffer[i++]];
					break;
			}
			break;
		default:
			s = mnemonic_main[buffer[i++]];
			break;
	}
	
	for (int j = 0; s[j]; ++j) {
		switch (s[j]) {
		case 'B':
			sprintf(buf, "#%02x", buffer[i++]);
			dest += buf;
			break;
		case 'R':
			sprintf(buf, "#%04x", (pc + 2 + (signed char)buffer[i++]) & 0xFFFF);
			dest += buf;
			break;
		case 'W':
			sprintf(buf, "#%04x", buffer[i] + buffer[i + 1] * 256);
			i += 2;
			dest += buf;
			break;
		case 'X':
			sprintf(buf, "(%s%c#%02x)", r, sign(buffer[i]), abs(buffer[i]));
			i++;
			dest += buf;
			break;
		case 'Y':
			sprintf(buf, "(%s%c#%02x)", r, sign(offset), abs(offset));
			dest += buf;
			break;
		case 'I':
			dest += r;
			break;
		case '!':
			sprintf(buf, "db     #ED,#%02x", buffer[1]);
			dest = buf;
			return 2;
		case '@':
			sprintf(buf, "db     #%02x", buffer[0]);
			dest = buf;
			return 1;
		case '#':
			sprintf(buf, "db     #%02x,#CB,#%02x", buffer[0], buffer[2]);
			dest = buf;
			return 2;
		case ' ': {
			dest.resize(7, ' ');
			break;
		}
		default:
			dest += s[j];
			break;
		}
	}
	dest.resize(18, ' ');
	return i;
}

} // namespace openmsx
