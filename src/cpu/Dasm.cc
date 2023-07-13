#include "Dasm.hh"
#include "DasmTables.hh"
#include "MSXCPUInterface.hh"
#include "narrow.hh"
#include "strCat.hh"

namespace openmsx {

static constexpr char sign(uint8_t a)
{
	return (a & 128) ? '-' : '+';
}

static constexpr int abs(uint8_t a)
{
	return (a & 128) ? (256 - a) : a;
}

void appendAddrAsHex(std::string& output, uint16_t addr)
{
	strAppend(output, '#', hex_string<4>(addr));
}

unsigned dasm(const MSXCPUInterface& interface, uint16_t pc, std::span<uint8_t, 4> buf,
              std::string& dest, EmuTime::param time,
              std::function<void(std::string&, uint16_t)> appendAddr)
{
	const char* r = nullptr;

	buf[0] = interface.peekMem(pc, time);
	auto [s, i] = [&]() -> std::pair<const char*, unsigned> {
		switch (buf[0]) {
			case 0xCB:
				buf[1] = interface.peekMem(pc + 1, time);
				return {mnemonic_cb[buf[1]], 2};
			case 0xED:
				buf[1] = interface.peekMem(pc + 1, time);
				return {mnemonic_ed[buf[1]], 2};
			case 0xDD:
			case 0xFD:
				r = (buf[0] == 0xDD) ? "ix" : "iy";
				buf[1] = interface.peekMem(pc + 1, time);
				if (buf[1] != 0xcb) {
					return {mnemonic_xx[buf[1]], 2};
				} else {
					buf[2] = interface.peekMem(pc + 2, time);
					buf[3] = interface.peekMem(pc + 3, time);
					return {mnemonic_xx_cb[buf[3]], 4};
				}
			default:
				return {mnemonic_main[buf[0]], 1};
		}
	}();

	for (int j = 0; s[j]; ++j) {
		switch (s[j]) {
		case 'B':
			buf[i] = interface.peekMem(narrow_cast<uint16_t>(pc + i), time);
			strAppend(dest, '#', hex_string<2>(
				static_cast<uint16_t>(buf[i])));
			i += 1;
			break;
		case 'R':
			buf[i] = interface.peekMem(narrow_cast<uint16_t>(pc + i), time);
			appendAddr(dest, pc + 2 + static_cast<int8_t>(buf[i]));
			i += 1;
			break;
		case 'A':
			buf[i + 0] = interface.peekMem(narrow_cast<uint16_t>(pc + i + 0), time);
			buf[i + 1] = interface.peekMem(narrow_cast<uint16_t>(pc + i + 1), time);
			appendAddr(dest, buf[i] + buf[i + 1] * 256);
			i += 2;
			break;
		case 'W':
			buf[i + 0] = interface.peekMem(narrow_cast<uint16_t>(pc + i + 0), time);
			buf[i + 1] = interface.peekMem(narrow_cast<uint16_t>(pc + i + 1), time);
			strAppend(dest, '#', hex_string<4>(buf[i] + buf[i + 1] * 256));
			i += 2;
			break;
		case 'X':
			buf[i] = interface.peekMem(narrow_cast<uint16_t>(pc + i), time);
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
	return i;
}

unsigned instructionLength(const MSXCPUInterface& interface, uint16_t pc,
                           EmuTime::param time)
{
	auto op0 = interface.peekMem(pc, time);
	auto t = instr_len_tab[op0];
	if (t < 4) return t;

	auto op1 = interface.peekMem(pc + 1, time);
	return instr_len_tab[64 * t + op1];
}

} // namespace openmsx
