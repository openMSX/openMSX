#include "Dasm.hh"
#include "DasmTables.hh"
#include "MSXCPUInterface.hh"
#include "strCat.hh"

namespace openmsx {

static constexpr char sign(unsigned char a)
{
	return (a & 128) ? '-' : '+';
}

static constexpr int abs(unsigned char a)
{
	return (a & 128) ? (256 - a) : a;
}

unsigned dasm(const MSXCPUInterface& interf, word pc, byte buf[4],
              std::string& dest, EmuTime::param time)
{
	const char* r = nullptr;

	buf[0] = interf.peekMem(pc, time);
	auto [s, i] = [&]() -> std::pair<const char*, unsigned> {
		switch (buf[0]) {
			case 0xCB:
				buf[1] = interf.peekMem(pc + 1, time);
				return {mnemonic_cb[buf[1]], 2};
			case 0xED:
				buf[1] = interf.peekMem(pc + 1, time);
				return {mnemonic_ed[buf[1]], 2};
			case 0xDD:
			case 0xFD:
				r = (buf[0] == 0xDD) ? "ix" : "iy";
				buf[1] = interf.peekMem(pc + 1, time);
				if (buf[1] != 0xcb) {
					return {mnemonic_xx[buf[1]], 2};
				} else {
					buf[2] = interf.peekMem(pc + 2, time);
					buf[3] = interf.peekMem(pc + 3, time);
					return {mnemonic_xx_cb[buf[3]], 4};
				}
			default:
				return {mnemonic_main[buf[0]], 1};
		}
	}();

	for (int j = 0; s[j]; ++j) {
		switch (s[j]) {
		case 'B':
			buf[i] = interf.peekMem(pc + i, time);
			strAppend(dest, '#', hex_string<2>(
				static_cast<uint16_t>(buf[i])));
			i += 1;
			break;
		case 'R':
			buf[i] = interf.peekMem(pc + i, time);
			strAppend(dest, '#', hex_string<4>(
				pc + 2 + static_cast<int8_t>(buf[i])));
			i += 1;
			break;
		case 'W':
			buf[i + 0] = interf.peekMem(pc + i + 0, time);
			buf[i + 1] = interf.peekMem(pc + i + 1, time);
			strAppend(dest, '#', hex_string<4>(buf[i] + buf[i + 1] * 256));
			i += 2;
			break;
		case 'X':
			buf[i] = interf.peekMem(pc + i, time);
			strAppend(dest, '(', r, sign(buf[i]), '#',
			     hex_string<2>(abs(buf[i])), ')');
			i += 1;
			break;
		case 'Y':
			strAppend(dest, r, sign(buf[2]), '#', hex_string<2>(abs(buf[2])));
			break;
		case 'I':
			dest += r;
			break;
		case '!':
			dest = strCat("db     #ED,#", hex_string<2>(buf[1]),
			              "     ");
			return 2;
		case '@':
			dest = strCat("db     #", hex_string<2>(buf[0]),
			              "         ");
			return 1;
		case '#':
			dest = strCat("db     #", hex_string<2>(buf[0]),
			              ",#CB,#", hex_string<2>(buf[2]),
			              ' ');
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
