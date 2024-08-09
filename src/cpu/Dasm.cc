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

constexpr std::array<uint8_t, 28> ddcbxx__ = {
	// keep'em in order
	0x06, 0x0e, 0x16, 0x1e, 0x46, 0x4e, 0x56, 0x5e, 0x66, 0x6e, 0x76, 0x7e, 0x86, 0x8e,
	0x96, 0x9e, 0xa6, 0xae, 0xb6, 0xbe, 0xc6, 0xce, 0xd6, 0xde, 0xe6, 0xee, 0xf6, 0xfe
};

std::optional<unsigned> instructionLength(std::span<const uint8_t> bin)
{
	if (bin.empty()) return {};
	auto t = instr_len_tab[bin[0]];
	if (t < 4) return t;

	if (bin.size() < 2) return {};
	t = instr_len_tab[64 * t + bin[1]];
	if (t > bin.size()) return {};
	if (bin.size() == 4 && !std::binary_search(ddcbxx__.begin(), ddcbxx__.end(), bin[3])) return {};
	return t;
}

unsigned dasm(std::span<const uint8_t> bin, uint16_t pc, std::string& dest,
              function_ref<void(std::string&, uint16_t)> appendAddr)
{
	const char* r = nullptr;

	auto [s, i] = [&]() -> std::pair<const char*, unsigned> {
		switch (bin[0]) {
			case 0xCB:
				return {mnemonic_cb[bin[1]], 2};
			case 0xED:
				return {mnemonic_ed[bin[1]], 2};
			case 0xDD:
			case 0xFD:
				r = (bin[0] == 0xDD) ? "ix" : "iy";
				if (bin[1] != 0xCB) {
					return {mnemonic_xx[bin[1]], 2};
				} else {
					return {mnemonic_xx_cb[bin[3]], 4};
				}
			default:
				return {mnemonic_main[bin[0]], 1};
		}
	}();

	for (int j = 0; s[j]; ++j) {
		switch (s[j]) {
		case 'B':
			strAppend(dest, '#', hex_string<2>(
				static_cast<uint16_t>(bin[i])));
			i += 1;
			break;
		case 'R':
			appendAddr(dest, uint16_t(pc + 2 + static_cast<int8_t>(bin[i])));
			i += 1;
			break;
		case 'A':
		case 'W':
			appendAddr(dest, bin[i] + bin[i + 1] * 256);
			i += 2;
			break;
		case 'X':
			strAppend(dest, '(', r, sign(bin[i]), '#',
			     hex_string<2>(abs(bin[i])), ')');
			i += 1;
			break;
		case 'Y':
			strAppend(dest, r, sign(bin[2]), '#', hex_string<2>(abs(bin[2])));
			break;
		case 'I':
			dest += r;
			break;
		case '!': // invalid z80 instruction
			dest = strCat("db     #ED,#", hex_string<2>(bin[1]),
			              "     ");
			return 0;
		case '@': // invalid z80 instruction
			dest = strCat("db     #", hex_string<2>(bin[0]),
			              "         ");
			return 0;
		case '#': // invalid z80 instruction
			dest = strCat("db     #", hex_string<2>(bin[0]),
			              ",#CB,#", hex_string<2>(bin[2]),
			              ' ');
			return 0;
		case ' ': {
			dest.resize(7, ' ');
			break;
		}
		default:
			dest += s[j];
			break;
		}
	}
	// used by unittest only
	return i;
}

std::span<uint8_t> fetchInstruction(const MSXCPUInterface& interface, uint16_t addr,
                                    std::span<uint8_t, 4> buffer, EmuTime::param time)
{
	uint16_t idx = 0;
	buffer[idx++] = interface.peekMem(addr, time);
	auto len = instr_len_tab[buffer[0]];
	if (len >= 4) {
		buffer[idx++] = interface.peekMem(addr + 1, time);
		len = instr_len_tab[64 * len + buffer[1]];
	}
	while (idx < len) {
		buffer[idx++] = interface.peekMem(addr + idx, time);
	}
	if (len == 4 && !std::binary_search(ddcbxx__.begin(), ddcbxx__.end(), buffer[3])) len = 3;
	return buffer.subspan(0, len);
}

unsigned dasm(const MSXCPUInterface& interface, uint16_t pc, std::span<uint8_t, 4> buf,
              std::string& dest, EmuTime::param time,
              function_ref<void(std::string&, uint16_t)> appendAddr)
{
	auto opcodes = fetchInstruction(interface, pc, buf, time);
	dasm(opcodes, pc, dest, appendAddr);
	assert(opcodes.size() > 0);
	return narrow_cast<unsigned>(opcodes.size());
}

static unsigned instructionLength(const MSXCPUInterface& interface, uint16_t pc,
                                  EmuTime::param time)
{
	auto op0 = interface.peekMem(pc, time);
	auto t = instr_len_tab[op0];
	if (t < 4) return t;

	auto op1 = interface.peekMem(pc + 1, time);
	t = instr_len_tab[64 * t + op1];

	auto op3 = interface.peekMem(pc + 3, time);
	if (!std::binary_search(ddcbxx__.begin(), ddcbxx__.end(), op3)) return 2;

	return t;
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
