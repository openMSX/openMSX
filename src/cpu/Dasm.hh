#ifndef DASM_HH
#define DASM_HH

#include "EmuTime.hh"

#include "function_ref.hh"

#include <cstdint>
#include <functional>
#include <span>
#include <string>

namespace openmsx {

class MSXCPUInterface;

void appendAddrAsHex(std::string& output, uint16_t addr);

/** Calculate the length of the instruction at the given address.
  * This is exactly the same value as calculated by the dasm() function above,
  * though this function executes much faster.
  * Note that this function (and also dasm()) ignores illegal instructions with
  * repeated prefixes (e.g. an instruction like 'DD DD DD DD DD 00'). Because of
  * this, the instruction length is always between 1 and 4 (inclusive).
  */
std::optional<unsigned> instructionLength(std::span<const uint8_t> bin);

std::span<uint8_t> fetchInstruction(const MSXCPUInterface& interface, uint16_t addr,
                                    std::span<uint8_t, 4> buffer, EmuTime time);

/** Disassemble
  * @param opcode Buffer containing the machine language instruction
                  The length of this buffer _must_ be exact. So typically
                  calculated via instructionLength() or fetchInstruction().
  * @param pc Program Counter used for disassembling relative addresses
  * @param dest String [output] representation of the disassembled opcode
  * @return Length of the disassembled opcode in bytes.
            Should always be the same as 'opcode.size()', so only useful in unittest.
  */
unsigned dasm(std::span<const uint8_t> opcode, uint16_t pc, std::string& dest,
              function_ref<void(std::string&, uint16_t)> appendAddr = &appendAddrAsHex);

/** Disassemble
  * @param interface The CPU interface, used to peek bytes from memory
  * @param pc The position (program counter) where to start disassembling
  * @param buf The bytes that form this opcode (max 4). The buffer is only
  *            filled with the min required bytes (see return value)
  * @param dest String representation of the disassembled opcode
  * @param time TODO
  * @return Length of the disassembled opcode in bytes
  */
unsigned dasm(const MSXCPUInterface& interface, uint16_t pc, std::span<uint8_t, 4> buf,
              std::string& dest, EmuTime time,
              function_ref<void(std::string&, uint16_t)> appendAddr = &appendAddrAsHex);

/** This is only an _heuristic_ to display instructions in a debugger disassembly
  * view. (In reality Z80 instruction can really start at _any_ address).
  *
  * Z80 instruction have a variable length between 1 and 4 bytes inclusive (we
  * ignore illegal instructions). If you were to start disassembling the full
  * address space from address 0, some addresses are at the start of an
  * instructions, others are not. To be able to show a 'stable' disassembly view
  * when scrolling up/down in the debugger, it's useful to know at which
  * addresses instructions start.
  *
  * This function can calculate such start addresses (on average) in a more
  * efficient way than just starting to disassemble from the top.
  *
  * More in detail: this function calculates the largest address smaller or
  * equal to a given address where an instruction starts. In other words: it
  * calculates the start address of the instruction that contains the given
  * address.
  */
uint16_t instructionBoundary(const MSXCPUInterface& interface, uint16_t addr,
                             EmuTime time);

/** Get the start address of the 'n'th instruction before the instruction
  * containing the byte at the given address 'addr'.
  * In other words, stepping 'n' instructions forwards from the resulting
  * address lands at the address that would be returned from
  * 'instuctionBoundary()'.
  * Unless there aren't enough instructions in front. In that case address 0 is
  * returned.
  */
uint16_t nInstructionsBefore(const MSXCPUInterface& interface, uint16_t addr,
                             EmuTime time, int n);

} // namespace openmsx

#endif
