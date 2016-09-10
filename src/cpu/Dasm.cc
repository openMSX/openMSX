#include "Dasm.hh"
#include "DasmTables.hh"
#include "MSXCPUInterface.hh"
#include "StringOp.hh"

namespace openmsx {

static char sign(unsigned char a)
{
	return (a & 128) ? '-' : '+';
}

static int abs(unsigned char a)
{
	return (a & 128) ? (256 - a) : a;
}

unsigned dasm(const MSXCPUInterface& interf, word pc, byte buf[4],
              std::string& dest, EmuTime::param time)
{
	const char* s;
	unsigned i = 0;
	const char* r = nullptr;

	buf[0] = interf.peekMem(pc, time);
	switch (buf[0]) {
		case 0xCB:
			buf[1] = interf.peekMem(pc + 1, time);
			s = mnemonic_cb[buf[1]];
			i = 2;
			break;
		case 0xED:
			buf[1] = interf.peekMem(pc + 1, time);
			s = mnemonic_ed[buf[1]];
			i = 2;
			break;
		case 0xDD:
		case 0xFD:
			r = (buf[0] == 0xDD) ? "ix" : "iy";
			buf[1] = interf.peekMem(pc + 1, time);
			if (buf[1] != 0xcb) {
				s = mnemonic_xx[buf[1]];
				i = 2;
			} else {
				buf[2] = interf.peekMem(pc + 2, time);
				buf[3] = interf.peekMem(pc + 3, time);
				s = mnemonic_xx_cb[buf[3]];
				i = 4;
			}
			break;
		default:
			s = mnemonic_main[buf[0]];
			i = 1;
	}

	for (int j = 0; s[j]; ++j) {
		switch (s[j]) {
		case 'B':
			buf[i] = interf.peekMem(pc + i, time);
			dest += '#' + StringOp::toHexString(
				static_cast<uint16_t>(buf[i]), 2);
			i += 1;
			break;
		case 'R':
			buf[i] = interf.peekMem(pc + i, time);
			dest += '#' + StringOp::toHexString(
				(pc + 2 + static_cast<int8_t>(buf[i])) & 0xFFFF, 4);
			i += 1;
			break;
		case 'W':
			buf[i + 0] = interf.peekMem(pc + i + 0, time);
			buf[i + 1] = interf.peekMem(pc + i + 1, time);
			dest += '#' + StringOp::toHexString(buf[i] + buf[i + 1] * 256, 4);
			i += 2;
			break;
		case 'X':
			buf[i] = interf.peekMem(pc + i, time);
			dest += '(' + std::string(r) + sign(buf[i]) + '#'
			     + StringOp::toHexString(abs(buf[i]), 2) + ')';
			i += 1;
			break;
		case 'Y':
			dest += std::string(r) + sign(buf[2]) + '#' + StringOp::toHexString(abs(buf[2]), 2);
			break;
		case 'I':
			dest += r;
			break;
		case '!':
			dest = "db     #ED,#" + StringOp::toHexString(buf[1], 2) +
			       "     ";
			return 2;
		case '@':
			dest = "db     #" + StringOp::toHexString(buf[0], 2) +
			       "         ";
			return 1;
		case '#':
			dest = "db     #" + StringOp::toHexString(buf[0], 2) +
			         ",#CB,#" + StringOp::toHexString(buf[2], 2) +
			         ' ';
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
	dest.resize(19, ' ');
	return i;
}

} // namespace openmsx
