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
              function_ref<void(std::string&, uint16_t)> appendAddr)
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
			appendAddr(dest, uint16_t(pc + 2 + static_cast<int8_t>(buf[i])));
			i += 1;
			break;
		case 'A':
		case 'W':
			buf[i + 0] = interface.peekMem(narrow_cast<uint16_t>(pc + i + 0), time);
			buf[i + 1] = interface.peekMem(narrow_cast<uint16_t>(pc + i + 1), time);
			appendAddr(dest, buf[i] + buf[i + 1] * 256);
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

// Calculates an address smaller or equal to the given 'addr' where there is for
// sure a boundary. Though this is not (yet) necessarily the largest address
// with this property.
// In addition return the length of the instruction at the resulting address.
static std::pair<uint16_t, unsigned> findGuaranteedBoundary(const MSXCPUInterface& interface, uint16_t addr, EmuTime::param time)
{
	if (addr < 3) {
		// address 0 (the top) is a boundary
		return {0, instructionLength(interface, 0, time)};
	}
	std::array<unsigned, 4> length;
	for (auto i : xrange(4)) {
		length[i] = instructionLength(interface, uint16_t(addr - i), time);
	}

	while (true) {
		// Criteria:
		//   * Assuming a maximum instruction length of 4:
		//   * IF  at 'addr - 1' starts an instruction with length 1
		//   * and at 'addr - 2' starts an instruction with length 1 or 2
		//   * and at 'addr - 3' starts an instruction with length 1 or 2 or 3
		//   * THEN 'addr' must be the start of an instruction.
		if ((length[1] == 1) && (length[2] <= 2) && (length[3] <= 3)) {
			return {addr, length[0]};
		}
		if (addr == 3) {
			// address 0 (the top) is a boundary
			return {0, length[3]};
		}
		--addr;
		length[0] = length[1];
		length[1] = length[2];
		length[2] = length[3];
		length[3] = instructionLength(interface, addr - 3, time);
	}
}

static std::pair<uint16_t, unsigned> instructionBoundaryAndLength(
	const MSXCPUInterface& interface, uint16_t addr, EmuTime::param time)
{
	// scan backwards for a guaranteed boundary
	auto [candidate, len] = findGuaranteedBoundary(interface, addr, time);
	// scan forwards for the boundary with the largest address
	while (true) {
		if ((candidate + len) > addr) return {candidate, len};
		candidate += len;
		len = instructionLength(interface, candidate, time);
	}
}

uint16_t instructionBoundary(const MSXCPUInterface& interface, uint16_t addr,
                             EmuTime::param time)
{
	auto [result, len] = instructionBoundaryAndLength(interface, addr, time);
	return result;
}

uint16_t nInstructionsBefore(const MSXCPUInterface& interface, uint16_t addr,
                             EmuTime::param time, int n)
{
	auto start = uint16_t(std::max(0, int(addr - 4 * n))); // for sure small enough
	auto [tmp, len] = instructionBoundaryAndLength(interface, start, time);

	std::vector<uint16_t> addresses;
	while ((tmp + len) <= addr) {
		addresses.push_back(tmp);
		tmp += len;
		len = instructionLength(interface, tmp, time);
	}
	addresses.push_back(tmp);

	return addresses[std::max(0, narrow<int>(addresses.size()) - 1 - n)];
}

} // namespace openmsx
